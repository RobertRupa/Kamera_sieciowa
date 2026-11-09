import { z } from 'zod'

export const wifiCredentialsSchema = z.object({
  ssid: z.string().refine(value => {
    const bytes = new TextEncoder().encode(value)
    return bytes.length > 0 && bytes.length <= 32 && !value.includes('\0')
  }),
  password: z.string().min(8).max(63).regex(/^[\x20-\x7e]+$/),
})

const sessionSchema = z.object({ data: z.object({
  token: z.string().min(32).max(256),
  expires_in_seconds: z.number().int().positive().max(300),
}) })
const apiErrorSchema = z.object({ error: z.object({ code: z.string().min(1).max(64) }) })
const provisioningStatusSchema = z.object({ data: z.object({
  wifi: z.enum(['ready', 'not_ready']),
  wifi_wizard: z.enum(['ready', 'not_ready']),
  credential_storage: z.enum(['ready', 'not_ready']),
  station_ip: z.ipv4().nullable(),
  provisioning_available: z.boolean(),
}) })
const operationSchema = z.object({ data: z.object({ operation_id: z.string().min(1).max(64) }) })
const scanStartSchema = z.object({ data: z.object({ operation_id: z.string().regex(/^scan-\d+$/) }) })
const scanSnapshotSchema = z.object({ data: z.object({
  operation_id: z.string().regex(/^scan-\d+$/),
  state: z.enum(['idle', 'scanning', 'complete', 'failed']),
  error: z.string().optional(),
  results: z.array(z.object({
    ssid: z.string().max(32),
    rssi: z.number().int().min(-127).max(10),
    security: z.enum(['open', 'wep', 'wpa_psk', 'wpa2_psk', 'wpa_wpa2_psk', 'enterprise', 'wpa3_psk', 'wpa2_wpa3_psk', 'owe', 'unknown']),
  })).max(20),
}) })
const wifiOperationSchema = z.object({ data: z.object({
  operation_id: z.string().regex(/^wifi-\d+$/),
  state: z.enum(['idle', 'queued', 'persisting', 'connecting', 'connected', 'failed']),
  station_ip: z.ipv4().nullable(),
}) })

export type WifiScanResult = z.infer<typeof scanSnapshotSchema>['data']['results'][number]
export type ProvisioningStatus = z.infer<typeof provisioningStatusSchema>['data']

async function provisioningRequest(base: string, path: string, method: string,
                                   body?: unknown, token?: string): Promise<unknown> {
  const response = await fetch(`${base}${path}`, {
    method,
    credentials: 'omit',
    redirect: 'error',
    signal: AbortSignal.timeout(5000),
    headers: { 'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}) },
    ...(body ? { body: JSON.stringify(body) } : {}),
  })
  if (!response.ok) {
    const responseBody = await response.json().catch(() => null)
    const apiError = apiErrorSchema.safeParse(responseBody)
    if (apiError.success) throw new Error(apiError.data.error.code)
    throw new Error(response.status === 401 || response.status === 403 ? 'unauthorized' : 'device_unavailable')
  }
  return response.json()
}

export async function createProvisioningSession(base: string) {
  return sessionSchema.parse(await provisioningRequest(base, '/api/v1/provisioning/session', 'POST')).data
}

export async function getProvisioningStatus(base: string) {
  return provisioningStatusSchema.parse(await provisioningRequest(base, '/api/v1/provisioning/status', 'GET')).data
}

export async function startWifiScan(base: string, token: string) {
  return scanStartSchema.parse(await provisioningRequest(base, '/api/v1/network/scan', 'POST', undefined, token)).data.operation_id
}

export async function getWifiScan(base: string, token: string, operationId: string) {
  const id = z.string().regex(/^scan-\d+$/).parse(operationId).slice(5)
  return scanSnapshotSchema.parse(await provisioningRequest(base, `/api/v1/network/scan/${id}`, 'GET', undefined, token)).data
}

export async function submitWifi(base: string, credentials: z.infer<typeof wifiCredentialsSchema>, token: string) {
  const validated = wifiCredentialsSchema.parse(credentials)
  return operationSchema.parse(await provisioningRequest(base, '/api/v1/network/credentials',
    'PUT', validated, token)).data.operation_id
}

export async function getWifiOperation(base: string, token: string, operationId: string) {
  const id = z.string().regex(/^wifi-\d+$/).parse(operationId).slice(5)
  return wifiOperationSchema.parse(await provisioningRequest(base, `/api/v1/network/operations/wifi-${id}`,
    'GET', undefined, token)).data
}