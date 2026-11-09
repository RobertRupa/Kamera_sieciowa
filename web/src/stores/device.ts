import { computed, ref } from 'vue'
import { defineStore } from 'pinia'
import { capabilitiesSchema, getDeviceResource, normalizeDeviceUrl, statusSchema } from '../api/client'
import type { Capabilities, DeviceStatus } from '../api/client'
import hardwareManifest from '../../../docs/hardware/board.json'

export const useDeviceStore = defineStore('device', () => {
  const reportedCamera = Object.freeze({
    module: hardwareManifest.camera.module,
    revision: hardwareManifest.camera.module_revision,
    sensors: Object.freeze([...hardwareManifest.camera.reported_sensors]),
  })
  const address = ref('http://camera-hub.local')
  const base = ref('')
  const status = ref<DeviceStatus | null>(null)
  const capabilities = ref<Capabilities | null>(null)
  const pending = ref(false)
  const error = ref('')
  const streamError = ref(false)
  const connected = computed(() => status.value !== null && capabilities.value !== null)
  const streamUrl = computed(() => {
    if (!connected.value || capabilities.value?.capabilities.http_mjpeg !== 'ready' || streamError.value) return ''
    const url = new URL(base.value)
    url.port = '81'
    url.pathname = '/stream.mjpg'
    return url.toString()
  })
  function state(feature: string) { return capabilities.value?.capabilities[feature] ?? 'unknown' }
  async function connect() {
    if (pending.value) return
    status.value = null
    capabilities.value = null
    streamError.value = false
    error.value = ''
    pending.value = true
    try {
      base.value = normalizeDeviceUrl(address.value)
      const [deviceStatus, deviceCapabilities] = await Promise.all([
        getDeviceResource(base.value, '/api/v1/device/status', statusSchema),
        getDeviceResource(base.value, '/api/v1/device/capabilities', capabilitiesSchema),
      ])
      status.value = deviceStatus.data
      capabilities.value = deviceCapabilities.data
    } catch (cause) {
      const code = cause instanceof Error ? cause.message : ''
      error.value = ['invalid_address', 'unauthorized'].includes(code) ? code : 'device_unavailable'
    } finally {
      pending.value = false
    }
  }
  return { reportedCamera, address, base, status, capabilities, pending, error, connected, streamError, streamUrl, state, connect }
})