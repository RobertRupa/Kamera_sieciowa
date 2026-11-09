import { describe, expect, it } from 'vitest'
import { capabilitiesSchema, normalizeDeviceUrl, statusSchema } from './client'
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