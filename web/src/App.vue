<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import Select from 'primevue/select'
import { useDeviceStore } from './stores/device'

const { t, locale } = useI18n()
const device = useDeviceStore()
const theme = ref('system')
const media = window.matchMedia('(prefers-color-scheme: dark)')
const languages = [{ label: 'English', value: 'en' }, { label: 'Polski', value: 'pl' }]
const themes = computed(() => ['system', 'light', 'dark'].map(value => ({ label: t(value), value })))
function applyTheme() {
  document.documentElement.classList.toggle('dark', theme.value === 'dark' || (theme.value === 'system' && media.matches))
}
watch(theme, applyTheme)
watch(locale, value => { document.documentElement.lang = value })
onMounted(() => { applyTheme(); media.addEventListener('change', applyTheme) })
onUnmounted(() => media.removeEventListener('change', applyTheme))
</script>

<template>
  <header class="topbar">
    <div class="brand"><i class="pi pi-camera" aria-hidden="true"></i><strong>camera_hub</strong><span>ESP32-P4</span></div>
    <div class="preferences">
      <Select v-model="locale" :options="languages" option-label="label" option-value="value" :aria-label="t('language')" />
      <Select v-model="theme" :options="themes" option-label="label" option-value="value" :aria-label="t('theme')" />
    </div>
  </header>
  <div class="app-layout">
    <nav class="navigation" :aria-label="t('navigation')">
      <RouterLink to="/"><i class="pi pi-video" aria-hidden="true"></i>{{ t('camera') }}</RouterLink>
      <RouterLink to="/diagnostics"><i class="pi pi-chart-bar" aria-hidden="true"></i>{{ t('diagnostics') }}</RouterLink>
      <RouterLink to="/wifi"><i class="pi pi-wifi" aria-hidden="true"></i>{{ t('wifi_setup') }}</RouterLink>
      <div class="connection-state"><span :class="['status-dot', { online: device.connected }]"></span>{{ t(device.connected ? 'connected' : 'disconnected') }}</div>
    </nav>
    <main><RouterView /></main>
  </div>
</template>
