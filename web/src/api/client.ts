import { z } from 'zod'

export function normalizeDeviceUrl(input: string): string {
  const url = new URL(input)
  if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password ||
      url.search || url.hash || url.pathname !== '/') {
    throw new Error('invalid_address')
  }
  return url.origin
}

const capabilityState = z.enum(['unsupported', 'unverified', 'disabled', 'not_ready', 'ready'])
export const capabilitiesSchema = z.object({ data: z.object({
  board_id: z.string().max(100),
  target: z.literal('esp32p4'),
  capabilities: z.record(z.string(), capabilityState),
}) })

export const statusSchema = z.object({ data: z.object({
  firmware_version: z.string().max(100),
  uptime_seconds: z.number().nonnegative(),
  free_heap_bytes: z.number().nonnegative(),
  camera: z.object({ width: z.number().int().nonnegative(), height: z.number().int().nonnegative(), fps: z.number().nonnegative() }).nullable(),
  camera_probe_state: z.enum(['not_run', 'no_supported_sensor', 'sensor_identified', 'frame_captured', 'capture_failed', 'error']).optional(),
  camera_hardware: z.object({
    sensor: z.string().min(1).max(32).nullable(),
    module: z.string().min(1).max(64).nullable(),
    module_revision: z.string().min(1).max(16).nullable(),
  }).nullable().optional(),
}) })

export type DeviceStatus = z.infer<typeof statusSchema>['data']
export type Capabilities = z.infer<typeof capabilitiesSchema>['data']

export async function getDeviceResource<T>(base: string, path: string, schema: z.ZodType<T>): Promise<T> {
  const response = await fetch(`${base}${path}`, {
    credentials: 'include',
    signal: AbortSignal.timeout(5000),
    headers: { Accept: 'application/json' },
    redirect: 'error',
  })
  if (!response.ok) throw new Error(response.status === 401 ? 'unauthorized' : 'device_unavailable')
  return schema.parse(await response.json())
}