import { describe, expect, it } from 'vitest'
import { capabilitiesSchema, normalizeDeviceUrl, statusSchema } from './client'
import { cameraInventorySchema, cameraSettingsSchema, cameraStreamLink } from './cameras'
import en from '../locales/en.json'
import pl from '../locales/pl.json'

describe('device boundary', () => {
  it('accepts an HTTP(S) origin', () => {
    expect(normalizeDeviceUrl('http://camera-hub.local/')).toBe('http://camera-hub.local')
    expect(normalizeDeviceUrl('https://192.168.1.2:443')).toBe('https://192.168.1.2')
  })
  it('rejects credentials, non-HTTP URLs and unexpected paths', () => {
    for (const url of ['ftp://camera.local', 'http://admin:password@camera.local', 'http://camera.local/api', 'http://camera.local/?token=secret', 'http://camera.local/#secret']) {
      expect(() => normalizeDeviceUrl(url)).toThrow()
    }
  })
  it('does not accept unknown ready claims', () => {
    expect(() => capabilitiesSchema.parse({ data: { board_id: 'test', target: 'esp32p4', capabilities: { wifi: true } } })).toThrow()
  })
  it('ships identical locale keys', () => {
    expect(Object.keys(en).sort()).toEqual(Object.keys(pl).sort())
  })
  it('rejects invalid camera settings', () => {
    expect(cameraSettingsSchema.parse({ mode_id: 0, preview_fps: 20, jpeg_quality: 80 })).toEqual({ mode_id: 0, preview_fps: 20, jpeg_quality: 80, codec: 'jpeg', h264_bitrate: 2000000 })
    for (const settings of [
      { mode_id: -1, preview_fps: 20, jpeg_quality: 80 },
      { mode_id: 0, preview_fps: 21, jpeg_quality: 80 },
      { mode_id: 0, preview_fps: 10, jpeg_quality: 101 },
    ]) expect(() => cameraSettingsSchema.parse(settings)).toThrow()
  })
  it('builds camera stream links only for active advertised streams', () => {
    const camera = { id: 'csi-0', sensor: 'OV5647', interface: 'MIPI-CSI', capturing: true,
      codec: 'jpeg' as const,
      mjpeg_path: '/stream.mjpg' as const, mjpeg_port: 81,
      rtsp_path: '/cameras/csi-0', rtsp_port: 8554 }
    expect(cameraStreamLink('http://192.168.1.2', camera, 'rtsp')).toBe('rtsp://192.168.1.2:8554/cameras/csi-0')
    expect(cameraStreamLink('http://192.168.1.2', camera, 'mjpeg')).toBe('http://192.168.1.2:81/stream.mjpg')
    expect(cameraStreamLink('http://192.168.1.2', { ...camera, capturing: false }, 'rtsp')).toBe('')
    expect(cameraStreamLink('http://192.168.1.2', { ...camera, rtsp_path: null }, 'rtsp')).toBe('')
    expect(() => cameraInventorySchema.parse({ data: { simultaneous_csi_inputs: 1, development_login: false,
      cameras: [{ ...camera, rtsp_path: '//external.example/stream' }] } })).toThrow()
  })
  it('accepts hardware H264 with a bounded bitrate, not arbitrary codec names', () => {
    expect(cameraSettingsSchema.parse({ mode_id: 0, preview_fps: 20, jpeg_quality: 80,
      codec: 'h264', h264_bitrate: 1000000 }).codec).toBe('h264')
    for (const change of [{ codec: 'h265' }, { h264_bitrate: 63999 }, { h264_bitrate: 10000001 }]) {
      expect(() => cameraSettingsSchema.parse({ mode_id: 0, preview_fps: 20, jpeg_quality: 80, ...change })).toThrow()
    }
  })
  it('keeps camera hardware unknown for legacy status responses', () => {
    const status = statusSchema.parse({ data: {
      firmware_version: '0.1.0', uptime_seconds: 0, free_heap_bytes: 0, camera: null,
    } })
    expect(status.data.camera_hardware).toBeUndefined()
  })
  it('accepts explicit unknown camera hardware from firmware', () => {
    const status = statusSchema.parse({ data: {
      firmware_version: '0.1.0', uptime_seconds: 0, free_heap_bytes: 0, camera: null,
      camera_probe_state: 'not_run', camera_hardware: null,
    } })
    expect(status.data.camera_hardware).toBeNull()
    expect(status.data.camera_probe_state).toBe('not_run')
  })
  it.each(['IMX219', 'OV5647'])('preserves a detected %s without requiring an active stream', sensor => {
    const status = statusSchema.parse({ data: {
      firmware_version: '0.1.0', uptime_seconds: 0, free_heap_bytes: 0, camera: null,
      camera_hardware: { sensor, module: 'UC-439', module_revision: 'B' },
    } })
    expect(status.data.camera_hardware?.sensor).toBe(sensor)
    expect(status.data.camera).toBeNull()
  })
})