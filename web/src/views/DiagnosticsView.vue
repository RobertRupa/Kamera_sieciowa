<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { useDeviceStore } from '../stores/device'

const { t } = useI18n()
const device = useDeviceStore()
</script>

<template>
  <div class="page-heading"><div><p class="eyebrow">ESP32-P4</p><h1>{{ t('diagnostics') }}</h1></div></div>
  <dl class="diagnostic-list">
    <div><dt>{{ t('board') }}</dt><dd>{{ device.capabilities?.board_id ?? t('unknown') }}</dd></div>
    <div><dt>{{ t('firmware') }}</dt><dd>{{ device.status?.firmware_version ?? t('unknown') }}</dd></div>
    <div><dt>{{ t('uptime') }}</dt><dd>{{ device.status ? `${device.status.uptime_seconds} s` : t('unknown') }}</dd></div>
    <div><dt>{{ t('heap') }}</dt><dd>{{ device.status ? `${device.status.free_heap_bytes} B` : t('unknown') }}</dd></div>
    <div><dt>{{ t('credential_storage') }}</dt><dd>{{ t(device.state('credential_storage')) }}</dd></div>
  </dl>
  <section class="diagnostics-section" :aria-label="t('camera')">
    <h2>{{ t('camera') }}</h2>
    <dl class="diagnostic-list">
      <div><dt>{{ t('reported_camera_module') }}</dt><dd>{{ device.reportedCamera.module }} rev.{{ device.reportedCamera.revision }}</dd></div>
      <div><dt>{{ t('reported_sensors') }}</dt><dd>{{ device.reportedCamera.sensors.join(' / ') }}</dd></div>
      <div><dt>{{ t('detected_camera_module') }}</dt><dd>{{ device.status?.camera_hardware?.module ?? t('unknown') }}<template v-if="device.status?.camera_hardware?.module && device.status.camera_hardware.module_revision"> rev.{{ device.status.camera_hardware.module_revision }}</template></dd></div>
      <div><dt>{{ t('detected_sensor') }}</dt><dd>{{ device.status?.camera_hardware?.sensor ?? t('unknown') }}</dd></div>
    </dl>
  </section>
  <table v-if="device.capabilities" class="capability-table"><thead><tr><th>{{ t('capability') }}</th><th>{{ t('state') }}</th></tr></thead><tbody><tr v-for="(state, key) in device.capabilities.capabilities" :key="key"><td>{{ key }}</td><td>{{ t(state) }}</td></tr></tbody></table>
</template>