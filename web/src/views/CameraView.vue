<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import Button from 'primevue/button'
import InputText from 'primevue/inputtext'
import Message from 'primevue/message'
import CameraControls from '../components/CameraControls.vue'
import { useDeviceStore } from '../stores/device'

const { t } = useI18n()
const device = useDeviceStore()
const cameraDetected = computed(() => {
  const hardware = device.status?.camera_hardware
  return Boolean(hardware?.module || hardware?.sensor || hardware?.module_revision)
})
const cameraDetectionStatus = computed(() => {
  if (!device.connected) return 'unknown'
  return cameraDetected.value ? 'camera_detected' : 'camera_detection_unknown'
})
const captureStatus = computed(() => {
  if (!device.connected) return 'unknown'
  return device.status?.camera ? 'camera_stream_active' : 'camera_stream_inactive'
})
const detectedModule = computed(() => {
  const hardware = device.status?.camera_hardware
  if (!hardware?.module) return t('unknown')
  return hardware.module_revision ? `${hardware.module} rev.${hardware.module_revision}` : hardware.module
})
const detectedSensor = computed(() => device.status?.camera_hardware?.sensor ?? t('unknown'))
const captureProfile = computed(() => {
  const camera = device.status?.camera
  return camera ? `${camera.width} × ${camera.height} · ${camera.fps} FPS` : t('unknown')
})
const cameraProbeStatus = computed(() => {
  const state = device.status?.camera_probe_state
  return device.connected && state ? `camera_probe_${state}` : 'unknown'
})
</script>

<template>
  <div class="page-heading"><div><p class="eyebrow">{{ t('live') }}</p><h1>{{ t('camera') }}</h1></div><Button icon="pi pi-refresh" :aria-label="t('refresh')" :title="t('refresh')" :loading="device.pending" @click="device.connect" /></div>
  <form class="device-connect" @submit.prevent="device.connect">
    <label for="device-address">{{ t('device_address') }}</label>
    <div class="input-row"><InputText id="device-address" v-model="device.address" type="url" required :disabled="device.pending" /><Button type="submit" icon="pi pi-link" :label="t('connect')" :loading="device.pending" /></div>
  </form>
  <Message v-if="device.error" severity="error" :closable="false" role="alert">{{ t(device.error) }}</Message>
  <div class="camera-layout">
    <section class="monitor" :aria-label="t('preview')">
      <div class="video-surface">
        <img v-if="device.streamUrl" :src="device.streamUrl" :alt="t('preview')" @error="device.streamError = true" />
        <div v-else class="video-empty"><i class="pi pi-video" aria-hidden="true"></i><strong>{{ t(device.pending ? 'connecting' : device.streamError ? 'stream_unavailable' : 'no_signal') }}</strong></div>
      </div>
      <div class="monitor-footer"><span><i class="pi pi-circle-fill" aria-hidden="true"></i> MJPEG</span><span v-if="device.status?.camera">{{ device.status.camera.width }} × {{ device.status.camera.height }} · {{ device.status.camera.fps }} FPS</span><span v-else>{{ t('unknown') }}</span></div>
    </section>
    <aside class="output-panel">
      <section :aria-label="t('camera_status')">
        <h2>{{ t('camera_status') }}</h2>
        <div class="output-row"><i class="pi pi-eye" aria-hidden="true"></i><div><strong>{{ t('camera_detection') }}</strong><span>{{ t(cameraDetectionStatus) }}</span></div></div>
        <div class="output-row"><i class="pi pi-camera" aria-hidden="true"></i><div><strong>{{ t('detected_camera_module') }}</strong><span>{{ detectedModule }}</span></div></div>
        <div class="output-row"><i class="pi pi-microchip" aria-hidden="true"></i><div><strong>{{ t('detected_sensor') }}</strong><span>{{ detectedSensor }}</span></div></div>
        <div class="output-row"><i class="pi pi-check-circle" aria-hidden="true"></i><div><strong>{{ t('camera_probe') }}</strong><span>{{ t(cameraProbeStatus) }}</span></div></div>
        <div class="output-row"><i class="pi pi-video" aria-hidden="true"></i><div><strong>{{ t('capture_status') }}</strong><span>{{ t(captureStatus) }}</span></div></div>
        <div class="output-row"><i class="pi pi-sliders-h" aria-hidden="true"></i><div><strong>{{ t('capture_profile') }}</strong><span>{{ captureProfile }}</span></div></div>
      </section>
      <h2>{{ t('outputs') }}</h2>
      <div class="output-row"><i class="pi pi-desktop" aria-hidden="true"></i><div><strong>USB UVC</strong><span>{{ t(device.state('usb_uvc')) }}</span></div></div>
      <div class="output-row"><i class="pi pi-globe" aria-hidden="true"></i><div><strong>HTTP MJPEG</strong><span>{{ t(device.state('http_mjpeg')) }}</span></div></div>
      <div class="output-row"><i class="pi pi-share-alt" aria-hidden="true"></i><div><strong>RTSP / RTP</strong><span>{{ t(device.state('rtsp')) }}</span></div></div>
      <h2>{{ t('network') }}</h2>
      <div class="output-row"><i class="pi pi-sitemap" aria-hidden="true"></i><div><strong>Ethernet</strong><span>{{ t(device.state('ethernet')) }}</span></div></div>
      <div class="output-row"><i class="pi pi-wifi" aria-hidden="true"></i><div><strong>Wi-Fi</strong><span>{{ t(device.state('wifi')) }}</span></div></div>
    </aside>
  </div>
  <CameraControls />
</template>