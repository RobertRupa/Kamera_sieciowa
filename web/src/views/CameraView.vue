<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import Button from 'primevue/button'
import InputText from 'primevue/inputtext'
import Message from 'primevue/message'
import { useDeviceStore } from '../stores/device'

const { t } = useI18n()
const device = useDeviceStore()
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
      <h2>{{ t('outputs') }}</h2>
      <div class="output-row"><i class="pi pi-desktop" aria-hidden="true"></i><div><strong>USB UVC</strong><span>{{ t(device.state('usb_uvc')) }}</span></div></div>
      <div class="output-row"><i class="pi pi-globe" aria-hidden="true"></i><div><strong>HTTP MJPEG</strong><span>{{ t(device.state('http_mjpeg')) }}</span></div></div>
      <div class="output-row"><i class="pi pi-share-alt" aria-hidden="true"></i><div><strong>RTSP / RTP</strong><span>{{ t(device.state('rtsp')) }}</span></div></div>
      <h2>{{ t('network') }}</h2>
      <div class="output-row"><i class="pi pi-sitemap" aria-hidden="true"></i><div><strong>Ethernet</strong><span>{{ t(device.state('ethernet')) }}</span></div></div>
      <div class="output-row"><i class="pi pi-wifi" aria-hidden="true"></i><div><strong>Wi-Fi</strong><span>{{ t(device.state('wifi')) }}</span></div></div>
    </aside>
  </div>
</template>