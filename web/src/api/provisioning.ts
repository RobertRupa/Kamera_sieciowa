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
const operationSchema = z.object({ data: z.object({ operation_id: z.string().min(1).max(64) }) })

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
  if (!response.ok) throw new Error(response.status === 401 || response.status === 403 ? 'unauthorized' : 'device_unavailable')
  return response.json()
}

export async function submitWifi(base: string, credentials: z.infer<typeof wifiCredentialsSchema>) {
  const validated = wifiCredentialsSchema.parse(credentials)
  const session = sessionSchema.parse(await provisioningRequest(base, '/api/v1/provisioning/session', 'POST'))
  return operationSchema.parse(await provisioningRequest(base, '/api/v1/network/credentials',
    'PUT', validated, session.data.token)).data.operation_id
}