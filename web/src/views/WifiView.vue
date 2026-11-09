<script setup lang="ts">
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import Button from 'primevue/button'
import InputText from 'primevue/inputtext'
import Password from 'primevue/password'
import Message from 'primevue/message'
import { useDeviceStore } from '../stores/device'
import { submitWifi, wifiCredentialsSchema } from '../api/provisioning'

const { t } = useI18n()
const device = useDeviceStore()
const ssid = ref('')
const password = ref('')
const pending = ref(false)
const error = ref('')
const operation = ref('')
const ready = computed(() => device.connected && device.state('wifi') === 'ready' &&
  device.state('wifi_wizard') === 'ready' && device.state('credential_storage') === 'ready')

async function connect() {
  if (!ready.value || pending.value) return
  error.value = ''
  operation.value = ''
  const parsed = wifiCredentialsSchema.safeParse({ ssid: ssid.value, password: password.value })
  if (!parsed.success) { error.value = 'invalid_wifi_credentials'; return }
  pending.value = true
  try {
    operation.value = await submitWifi(device.base, parsed.data)
  } catch (cause) {
    error.value = cause instanceof Error && cause.message === 'unauthorized' ? 'unauthorized' : 'device_unavailable'
  } finally {
    password.value = ''
    pending.value = false
  }
}
</script>

<template>
  <div class="page-heading"><div><p class="eyebrow">Wi-Fi / ESP32-C6</p><h1>{{ t('wifi_setup') }}</h1></div><Button icon="pi pi-refresh" :aria-label="t('refresh')" :title="t('refresh')" :loading="device.pending" @click="device.connect" /></div>
  <Message v-if="!ready" severity="warn" :closable="false">{{ t(device.connected ? 'wifi_setup_blocked' : 'device_connection_required') }}</Message>
  <dl class="diagnostic-list">
    <div><dt>Wi-Fi</dt><dd>{{ t(device.state('wifi')) }}</dd></div>
    <div><dt>{{ t('wifi_setup') }}</dt><dd>{{ t(device.state('wifi_wizard')) }}</dd></div>
    <div><dt>{{ t('credential_storage') }}</dt><dd>{{ t(device.state('credential_storage')) }}</dd></div>
    <div><dt>mDNS</dt><dd>{{ t(device.state('mdns')) }}</dd></div>
  </dl>
  <form v-if="ready" class="wifi-form" @submit.prevent="connect">
    <Message severity="warn" :closable="false">{{ t('wifi_plaintext_warning') }}</Message>
    <label for="wifi-ssid">SSID</label>
    <InputText id="wifi-ssid" v-model="ssid" required :disabled="pending" autocomplete="off" />
    <label for="wifi-password">{{ t('wifi_password') }}</label>
    <Password input-id="wifi-password" v-model="password" :feedback="false" toggle-mask :disabled="pending" autocomplete="new-password" />
    <Message v-if="error" severity="error" :closable="false">{{ t(error) }}</Message>
    <Message v-if="operation" severity="info" :closable="false">{{ t('wifi_operation_pending') }}: {{ operation }}</Message>
    <Button type="submit" icon="pi pi-wifi" :label="t('connect')" :loading="pending" />
  </form>
</template>