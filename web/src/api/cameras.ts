import { z } from 'zod'

const cameraId = z.string().regex(/^[a-z0-9-]{1,32}$/)
export const cameraInventorySchema = z.object({ data: z.object({
  simultaneous_csi_inputs: z.number().int().min(0).max(4),
  development_login: z.boolean(),
  cameras: z.array(z.object({
    id: cameraId,
    sensor: z.string().min(1).max(32),
    interface: z.string().max(32),
    capturing: z.boolean(),
    codec: z.enum(['jpeg', 'h264']).default('jpeg'),
    mjpeg_path: z.literal('/stream.mjpg').nullable(),
    mjpeg_port: z.number().int().min(1).max(65535),
    rtsp_path: z.string().regex(/^\/cameras\/[a-z0-9-]{1,32}$/).nullable(),
    rtsp_port: z.number().int().min(1).max(65535),
  })).max(4),
}) })

export const cameraSettingsSchema = z.object({
  mode_id: z.number().int().min(0).max(31),
  preview_fps: z.number().int().min(1).max(20),
  jpeg_quality: z.number().int().min(1).max(100),
  codec: z.enum(['jpeg', 'h264']).default('jpeg'),
  h264_bitrate: z.number().int().min(64000).max(10000000).default(2000000),
})
export const cameraConfigurationSchema = z.object({ data: z.object({
  persistent: z.boolean(),
  hardware_codecs: z.array(z.enum(['jpeg', 'h264'])).max(2).default(['jpeg']),
  state: z.enum(['idle', 'queued', 'applying', 'applied', 'failed']),
  settings: cameraSettingsSchema,
  modes: z.array(z.object({
    id: z.number().int().min(0).max(31),
    name: z.string().max(79),
    width: z.number().int().min(1).max(65535),
    height: z.number().int().min(1).max(65535),
    sensor_fps: z.number().int().min(1).max(65535),
    available: z.boolean(),
    h264_available: z.boolean().default(false),
  })).max(32),
}) })
const udpSchema = z.object({ data: z.object({
  ready: z.boolean(), enabled: z.boolean(),
  codec: z.enum(['jpeg', 'h264']).default('jpeg'),
  receiver: z.union([z.ipv4(), z.literal('')]),
  port: z.number().int().min(1024).max(65534),
  frames_sent: z.number().int().nonnegative(),
  oversized_frames: z.number().int().nonnegative(),
  send_errors: z.number().int().nonnegative(),
  stale_frames: z.number().int().nonnegative().optional(),
  last_frame_age_us: z.number().int().nonnegative().optional(),
  max_frame_age_us: z.number().int().nonnegative().optional(),
  last_send_duration_us: z.number().int().nonnegative().optional(),
  max_send_duration_us: z.number().int().nonnegative().optional(),
}) })
const sessionSchema = z.object({ data: z.object({
  token: z.string().regex(/^[a-f0-9]{64}$/),
  expires_in_seconds: z.number().int().min(1).max(900),
}) })

export type CameraInventory = z.infer<typeof cameraInventorySchema>['data']
export type CameraConfiguration = z.infer<typeof cameraConfigurationSchema>['data']
export type CameraSettings = z.infer<typeof cameraSettingsSchema>
export type CameraUdp = z.infer<typeof udpSchema>['data']

async function request(base: string, path: string, method = 'GET', body?: unknown, token?: string) {
  const response = await fetch(`${base}${path}`, {
    method, credentials: 'omit', redirect: 'error', signal: AbortSignal.timeout(5000),
    headers: { Accept: 'application/json', 'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}) },
    ...(body === undefined ? {} : { body: JSON.stringify(body) }),
  })
  if (!response.ok) throw new Error(response.status === 401 ? 'unauthorized' : 'camera_configuration_failed')
  return response.status === 204 ? null : response.json()
}

function path(id: string) { return `/api/v1/cameras/${cameraId.parse(id)}` }
export async function getCameras(base: string) {
  return cameraInventorySchema.parse(await request(base, '/api/v1/cameras')).data
}
export async function getCameraConfiguration(base: string, id: string) {
  return cameraConfigurationSchema.parse(await request(base, path(id))).data
}
export async function saveCameraConfiguration(base: string, id: string, settings: CameraSettings, token: string) {
  return cameraConfigurationSchema.parse(await request(base, `${path(id)}/config`, 'PATCH',
    cameraSettingsSchema.parse(settings), token)).data
}
export async function cameraLogin(base: string, username: string, password: string) {
  return sessionSchema.parse(await request(base, '/api/v1/camera/session', 'POST', { username, password })).data
}
export async function cameraLogout(base: string, token: string) {
  await request(base, '/api/v1/camera/session', 'DELETE', undefined, token)
}
export async function getCameraUdp(base: string, id: string) {
  return udpSchema.parse(await request(base, `${path(id)}/udp`)).data
}
export async function saveCameraUdp(base: string, id: string, enabled: boolean, port: number, token: string) {
  z.number().int().min(1024).max(65534).refine(value => value % 2 === 0).parse(port)
  return udpSchema.parse(await request(base, `${path(id)}/udp`, 'PUT', { enabled, port }, token)).data
}

export function cameraStreamLink(base: string, camera: CameraInventory['cameras'][number], kind: 'mjpeg' | 'rtsp') {
  const streamPath = kind === 'rtsp' ? camera.rtsp_path : camera.mjpeg_path
  if (!camera.capturing || !streamPath) return ''
  const url = new URL(base)
  if (kind === 'rtsp') {
    return `rtsp://${url.hostname}:${camera.rtsp_port}${streamPath}`
  }
  url.port = String(camera.mjpeg_port)
  url.pathname = streamPath
  return url.toString()
}