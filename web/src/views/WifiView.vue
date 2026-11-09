<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import Button from 'primevue/button'
import InputText from 'primevue/inputtext'
import Password from 'primevue/password'
import Message from 'primevue/message'
import Select from 'primevue/select'
import { useDeviceStore } from '../stores/device'
import { createProvisioningSession, getProvisioningStatus, getWifiOperation, getWifiScan, startWifiScan, submitWifi, wifiCredentialsSchema } from '../api/provisioning'
import type { ProvisioningStatus, WifiScanResult } from '../api/provisioning'

const { t } = useI18n()
const device = useDeviceStore()
const provisioningBase = ref('')
const provisioningStatus = ref<ProvisioningStatus | null>(null)
const statusLoading = ref(false)
const statusError = ref('')
const stationIp = ref('')
const ssid = ref('')
const password = ref('')
const pending = ref(false)
const scanPending = ref(false)
const error = ref('')
const operation = ref('')
const operationState = ref('')
const scanOperation = ref('')
const sessionToken = ref('')
const sessionExpiresAt = ref(0)
const scannedNetworks = ref<WifiScanResult[]>([])
const selectedNetwork = ref<WifiScanResult | null>(null)
const ready = computed(() => provisioningStatus.value?.wifi === 'ready' && provisioningStatus.value.wifi_wizard === 'ready')
const credentialsReady = computed(() => ready.value && provisioningStatus.value?.credential_storage === 'ready')
const provisioningAvailable = computed(() => provisioningStatus.value?.provisioning_available === true)
const stationUrl = computed(() => stationIp.value ? `http://${stationIp.value}/` : '')
const networkOptions = computed(() => scannedNetworks.value.map(network => ({
  ...network,
  label: `${network.ssid || t('wifi_hidden_network')} (${network.rssi} dBm, ${t(`wifi_security_${network.security}`)})`,
  disabled: !['wpa2_psk', 'wpa_wpa2_psk', 'wpa3_psk', 'wpa2_wpa3_psk'].includes(network.security),
})))
const scanErrorMessages: Record<string, string> = {
  scan_busy: 'wifi_scan_busy',
  wifi_not_ready: 'wifi_not_ready',
  scan_unavailable: 'wifi_scan_unavailable',
  unauthorized: 'unauthorized',
  wifi_scan_failed: 'wifi_scan_failed',
  wifi_scan_timeout: 'wifi_scan_timeout',
}

watch(selectedNetwork, network => {
  if (network?.ssid) ssid.value = network.ssid
})

async function refreshProvisioning() {
  statusLoading.value = true
  statusError.value = ''
  provisioningBase.value = device.base || window.location.origin
  try {
    provisioningStatus.value = await getProvisioningStatus(provisioningBase.value)
    stationIp.value = provisioningStatus.value.station_ip ?? ''
  } catch (cause) {
    provisioningStatus.value = null
    statusError.value = cause instanceof Error && cause.message === 'unauthorized' ? 'unauthorized' : 'device_unavailable'
  } finally {
    statusLoading.value = false
  }
}

onMounted(refreshProvisioning)

async function ensureSession() {
  if (sessionToken.value && Date.now() < sessionExpiresAt.value - 5000) return sessionToken.value
  const session = await createProvisioningSession(provisioningBase.value)
  sessionToken.value = session.token
  sessionExpiresAt.value = Date.now() + session.expires_in_seconds * 1000
  return session.token
}

async function scanNetworks() {
  if (!ready.value || !provisioningAvailable.value || pending.value || scanPending.value) return
  error.value = ''
  scannedNetworks.value = []
  selectedNetwork.value = null
  scanOperation.value = ''
  scanPending.value = true
  try {
    const token = await ensureSession()
    scanOperation.value = await startWifiScan(provisioningBase.value, token)
    const deadline = Date.now() + 25000
    while (Date.now() < deadline) {
      await new Promise<void>(resolve => window.setTimeout(resolve, 600))
      const result = await getWifiScan(provisioningBase.value, token, scanOperation.value)
      if (result.state === 'failed') throw new Error('wifi_scan_failed')
      if (result.state === 'complete') {
        scannedNetworks.value = result.results
        if (result.results.length === 0) error.value = 'wifi_scan_empty'
        return
      }
    }
    throw new Error('wifi_scan_timeout')
  } catch (cause) {
    const code = cause instanceof Error ? cause.message : ''
    error.value = scanErrorMessages[code] ?? 'device_unavailable'
  } finally {
    scanPending.value = false
  }
}

async function connect() {
  if (!credentialsReady.value || !provisioningAvailable.value || pending.value || scanPending.value) return
  error.value = ''
  operation.value = ''
  operationState.value = ''
  const parsed = wifiCredentialsSchema.safeParse({ ssid: ssid.value, password: password.value })
  if (!parsed.success) { error.value = 'invalid_wifi_credentials'; return }
  pending.value = true
  try {
    const token = await ensureSession()
    operation.value = await submitWifi(provisioningBase.value, parsed.data, token)
    const deadline = Date.now() + 35000
    while (Date.now() < deadline) {
      const result = await getWifiOperation(provisioningBase.value, token, operation.value)
      operationState.value = result.state
      if (result.station_ip) stationIp.value = result.station_ip
      if (result.state === 'connected') return
      if (result.state === 'failed') throw new Error('wifi_connection_failed')
      await new Promise<void>(resolve => window.setTimeout(resolve, 800))
    }
    throw new Error('wifi_connection_timeout')
  } catch (cause) {
    error.value = cause instanceof Error && cause.message.startsWith('wifi_connection_') ? cause.message :
      cause instanceof Error && cause.message === 'unauthorized' ? 'unauthorized' : 'device_unavailable'
  } finally {
    password.value = ''
    pending.value = false
  }
}
</script>

<template>
  <div class="page-heading"><div><p class="eyebrow">Wi-Fi / ESP32-C6</p><h1>{{ t('wifi_setup') }}</h1></div><Button icon="pi pi-refresh" :aria-label="t('refresh')" :title="t('refresh')" :loading="statusLoading" @click="refreshProvisioning" /></div>
  <Message v-if="statusError" severity="error" :closable="false">{{ t(statusError) }}</Message>
  <Message v-else-if="!ready" severity="warn" :closable="false">{{ t(provisioningStatus ? 'wifi_setup_blocked' : 'device_connection_required') }}</Message>
  <dl v-if="provisioningStatus" class="diagnostic-list">
    <div><dt>Wi-Fi</dt><dd>{{ t(provisioningStatus.wifi) }}</dd></div>
    <div><dt>{{ t('wifi_setup') }}</dt><dd>{{ t(provisioningStatus.wifi_wizard) }}</dd></div>
    <div><dt>{{ t('credential_storage') }}</dt><dd>{{ t(provisioningStatus.credential_storage) }}</dd></div>
    <div v-if="stationIp"><dt>{{ t('wifi_station_ip') }}</dt><dd>{{ stationIp }}</dd></div>
  </dl>
  <Message v-if="stationUrl" severity="success" :closable="false">
    {{ t('wifi_station_ip') }}: <a :href="stationUrl">{{ stationIp }}</a>
  </Message>
  <Message v-if="ready && !provisioningAvailable" severity="info" :closable="false">{{ t('wifi_provisioning_ap_only') }}</Message>
  <form v-if="ready && provisioningAvailable" class="wifi-form" @submit.prevent="connect">
    <Message severity="warn" :closable="false">{{ t('wifi_plaintext_warning') }}</Message>
    <Message v-if="!credentialsReady" severity="warn" :closable="false">{{ t('wifi_credentials_unavailable') }}</Message>
    <div class="wifi-scan">
      <Button type="button" icon="pi pi-search" :label="t('wifi_scan')" :loading="scanPending" :disabled="pending" @click="scanNetworks" />
      <Select v-model="selectedNetwork" :options="networkOptions" option-label="label" option-disabled="disabled"
        :placeholder="t('wifi_select_network')" :disabled="scanPending || pending || networkOptions.length === 0" />
    </div>
    <Message v-if="scanPending" severity="info" :closable="false">{{ t('wifi_scan_pending') }}</Message>
    <label for="wifi-ssid">SSID</label>
    <InputText id="wifi-ssid" v-model="ssid" required :disabled="pending || scanPending" autocomplete="off" />
    <label for="wifi-password">{{ t('wifi_password') }}</label>
    <Password input-id="wifi-password" v-model="password" :feedback="false" toggle-mask :disabled="pending || scanPending" autocomplete="new-password" />
    <Message v-if="error" severity="error" :closable="false">{{ t(error) }}</Message>
    <Message v-if="operation" severity="info" :closable="false">{{ t(`wifi_operation_${operationState || 'queued'}`) }}: {{ operation }}</Message>
    <Button type="submit" icon="pi pi-wifi" :label="t('connect')" :loading="pending" :disabled="scanPending || !credentialsReady" />
  </form>
</template>

<style scoped>
.wifi-scan {
  display: grid;
  grid-template-columns: minmax(10rem, 0.8fr) minmax(0, 1.5fr);
  gap: 0.75rem;
}

@media (max-width: 640px) {
  .wifi-scan {
    grid-template-columns: minmax(0, 1fr);
  }
}
</style>