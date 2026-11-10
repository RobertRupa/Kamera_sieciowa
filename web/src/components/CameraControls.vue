<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import Button from 'primevue/button'
import InputText from 'primevue/inputtext'
import InputNumber from 'primevue/inputnumber'
import Select from 'primevue/select'
import ToggleSwitch from 'primevue/toggleswitch'
import Message from 'primevue/message'
import { useDeviceStore } from '../stores/device'
import { cameraLogin, cameraLogout, cameraStreamLink, getCameras, getCameraConfiguration,
  getCameraUdp, saveCameraConfiguration, saveCameraUdp } from '../api/cameras'
import type { CameraInventory, CameraConfiguration, CameraUdp } from '../api/cameras'

const { t } = useI18n()
const device = useDeviceStore()
const inventory = ref<CameraInventory | null>(null)
const configuration = ref<CameraConfiguration | null>(null)
const udp = ref<CameraUdp | null>(null)
const selected = ref('')
const modeId = ref(0)
const quality = ref(80)
const codec = ref<'jpeg' | 'h264'>('jpeg')
const bitrate = ref(2000000)
const udpEnabled = ref(false)
const udpPort = ref(5004)
const username = ref('admin')
const password = ref('')
const token = ref('')
const expiresAt = ref(0)
const pending = ref(false)
const message = ref('')
const editing = ref(false)
let syncingControls = false
let generation = 0
const cameras = computed(() => inventory.value?.cameras.map(camera => ({ ...camera,
  label: `${camera.id} · ${camera.sensor} · ${camera.interface}` })) ?? [])
const modes = computed(() => configuration.value?.modes.map(mode => ({ ...mode,
  label: `${mode.width} × ${mode.height} · ${mode.sensor_fps} FPS`, disabled: !mode.available })) ?? [])
const codecOptions = computed(() => (configuration.value?.hardware_codecs ?? ['jpeg']).map(value => ({
  value, label: t(value === 'h264' ? 'hardware_h264' : 'hardware_jpeg'),
  disabled: value === 'h264' && !modes.value.find(mode => mode.id === modeId.value)?.h264_available,
})))
const compatibleCodec = computed(() => codec.value !== 'h264' || modes.value.find(mode => mode.id === modeId.value)?.h264_available)
const usbStreaming = computed(() => configuration.value?.usb_streaming === true)
const sdpLink = computed(() => `${device.base}/api/v1/cameras/${selected.value}/stream.sdp`)

watch([modeId, quality, codec, bitrate], () => {
  if (!syncingControls) editing.value = true
}, { flush: 'sync' })

function acceptConfiguration(value: CameraConfiguration) {
  configuration.value = value
  syncingControls = true
  modeId.value = value.settings.mode_id
  quality.value = value.settings.jpeg_quality
  codec.value = value.settings.codec
  bitrate.value = value.settings.h264_bitrate
  editing.value = false
  syncingControls = false
}

async function refreshCamera() {
  if (!selected.value) return
  const current = generation
  try {
    const cameraId = selected.value
    const config = await getCameraConfiguration(device.base, cameraId)
    if (current !== generation || cameraId !== selected.value) return
    acceptConfiguration(config)
    // UDP/RTSP availability must not prevent changing the sensor mode.
    try {
      const network = await getCameraUdp(device.base, cameraId)
      if (current !== generation || cameraId !== selected.value) return
      udp.value = network
      udpEnabled.value = network.enabled
      udpPort.value = network.port
    } catch { udp.value = null }
  } catch { if (current === generation) message.value = 'camera_configuration_unavailable' }
}

watch(() => device.connected ? device.base : '', async base => {
  ++generation
  const current = generation
  inventory.value = null
  configuration.value = null
  editing.value = false
  udp.value = null
  token.value = ''
  password.value = ''
  message.value = ''
  selected.value = ''
  if (!base) return
  try {
    const value = await getCameras(base)
    if (current !== generation) return
    inventory.value = value
    selected.value = value.cameras[0]?.id ?? ''
  } catch { if (current === generation) message.value = 'camera_configuration_unavailable' }
}, { immediate: true })

watch(selected, refreshCamera)
// UVC COMMIT and USB disconnect change camera settings independently of the UI.
// Poll the device while preserving unsaved edits in the form.
let cameraPollPending = false
const cameraPollTimer = setInterval(async () => {
  if (cameraPollPending || pending.value || !device.connected || !selected.value) return
  const current = generation
  const cameraId = selected.value
  cameraPollPending = true
  try {
    const snapshot = await getCameraConfiguration(device.base, cameraId)
    if (current !== generation || cameraId !== selected.value) return
    if (editing.value) configuration.value = snapshot
    else acceptConfiguration(snapshot)
    await device.refreshStatus(false)
  } catch {
    // Keep the last known state; the next polling interval retries.
  } finally { cameraPollPending = false }
}, 2000)
let udpPollPending = false
const udpPollTimer = setInterval(async () => {
  if (udpPollPending || pending.value || !device.connected || !selected.value || !udp.value?.enabled) return
  const current = generation
  udpPollPending = true
  try {
    const snapshot = await getCameraUdp(device.base, selected.value)
    if (current === generation) udp.value = snapshot
  } catch {
    if (current === generation) message.value = 'camera_configuration_unavailable'
  } finally { udpPollPending = false }
}, 1000)
onBeforeUnmount(() => {
  clearInterval(cameraPollTimer)
  clearInterval(udpPollTimer)
  ++generation
  token.value = ''
  password.value = ''
})

async function action(work: () => Promise<void>) {
  if (pending.value) return
  pending.value = true
  message.value = ''
  try { await work() }
  catch (cause) {
    message.value = cause instanceof Error && ['unauthorized', 'camera_busy'].includes(cause.message)
      ? cause.message : 'camera_configuration_failed'
    if (message.value === 'unauthorized') token.value = ''
  } finally { pending.value = false; password.value = '' }
}

function login() {
  return action(async () => {
    const session = await cameraLogin(device.base, username.value, password.value)
    token.value = session.token
    expiresAt.value = Date.now() + session.expires_in_seconds * 1000
  })
}
function logout() {
  return action(async () => {
    const session = token.value
    token.value = ''
    await cameraLogout(device.base, session)
    await refreshCamera()
  })
}
function requireToken() {
  if (!token.value || Date.now() >= expiresAt.value) { token.value = ''; throw new Error('unauthorized') }
  return token.value
}
function apply() {
  return action(async () => {
    const current = generation
    await saveCameraConfiguration(device.base, selected.value,
      { mode_id: modeId.value, jpeg_quality: quality.value,
        codec: codec.value, h264_bitrate: bitrate.value }, requireToken())
    for (let attempt = 0; attempt < 40 && current === generation; ++attempt) {
      await new Promise(resolve => setTimeout(resolve, 500))
      const value = await getCameraConfiguration(device.base, selected.value)
      if (current !== generation) return
      configuration.value = value
      if (value.state === 'failed') throw new Error('camera_configuration_failed')
      if (value.state === 'applied') {
        acceptConfiguration(value)
        // RTSP/UDP may be disabled while camera configuration is still valid.
        // Report camera success independently from network-stream availability.
        if (current !== generation) return
        inventory.value = await getCameras(device.base)
        await refreshCamera()
        await device.refreshStatus()
        message.value = 'camera_configuration_applied'
        return
      }
    }
    if (current === generation) throw new Error('camera_configuration_failed')
  })
}
function applyUdp() {
  return action(async () => {
    udp.value = await saveCameraUdp(device.base, selected.value, udpEnabled.value, udpPort.value, requireToken())
  })
}
</script>

<template>
  <section v-if="device.connected" class="camera-controls" :aria-label="t('camera_configuration')">
    <h2>{{ t('camera_configuration') }}</h2>
    <Message v-if="message" :severity="message === 'camera_configuration_applied' ? 'success' : 'warn'" :closable="false">{{ t(message) }}</Message>
    <template v-if="inventory">
      <div v-for="camera in cameras" :key="camera.id" class="camera-stream-links">
        <strong>{{ camera.id }} · {{ camera.sensor }} · {{ camera.interface }}</strong>
        <a v-if="cameraStreamLink(device.base, camera, 'mjpeg')" :href="cameraStreamLink(device.base, camera, 'mjpeg')"><i class="pi pi-external-link" aria-hidden="true"></i> MJPEG</a>
        <a v-if="cameraStreamLink(device.base, camera, 'rtsp')" :href="cameraStreamLink(device.base, camera, 'rtsp')"><i class="pi pi-share-alt" aria-hidden="true"></i> RTSP / UDP</a>
      </div>
      <div v-if="cameras.length" class="camera-settings-grid">
        <label for="camera-input">{{ t('camera_input') }}</label>
        <Select id="camera-input" v-model="selected" :aria-label="t('camera_input')" :options="cameras" option-label="label" option-value="id" :disabled="pending" />
      </div>
      <Message v-if="inventory.development_login" severity="warn" :closable="false">{{ t('camera_development_warning') }}</Message>
      <Message v-else severity="warn" :closable="false">{{ t('camera_login_disabled') }}</Message>
      <Message v-if="usbStreaming" severity="info" :closable="false">{{ t('camera_usb_locked') }}</Message>
      <form v-if="inventory.development_login && !token" class="camera-login" @submit.prevent="login">
        <label for="camera-username">{{ t('username') }}</label>
        <InputText id="camera-username" v-model="username" autocomplete="username" maxlength="32" :disabled="pending" required />
        <label for="camera-password">{{ t('password') }}</label>
        <InputText id="camera-password" v-model="password" type="password" autocomplete="current-password" maxlength="64" :disabled="pending" required />
        <Button type="submit" icon="pi pi-sign-in" :label="t('login')" :loading="pending" />
      </form>
      <Button v-if="token" icon="pi pi-sign-out" :label="t('logout')" :disabled="pending" @click="logout" />
      <form v-if="configuration" class="camera-settings-grid" @submit.prevent="apply">
        <label for="camera-mode">{{ t('sensor_mode') }}</label>
        <Select id="camera-mode" v-model="modeId" :aria-label="t('sensor_mode')" :options="modes" option-label="label" option-value="id" option-disabled="disabled" :disabled="pending || !token || usbStreaming" />
        <label for="camera-codec">{{ t('hardware_codec') }}</label>
        <Select id="camera-codec" v-model="codec" :aria-label="t('hardware_codec')" :options="codecOptions" option-label="label" option-value="value" option-disabled="disabled" :disabled="pending || !token || usbStreaming" />
        <template v-if="codec === 'jpeg'">
          <label for="camera-quality">{{ t('jpeg_quality') }}</label>
          <InputNumber input-id="camera-quality" v-model="quality" :min="1" :max="100" show-buttons :disabled="pending || !token || usbStreaming" />
        </template>
        <template v-else>
          <label for="camera-bitrate">{{ t('h264_bitrate') }}</label>
          <InputNumber input-id="camera-bitrate" v-model="bitrate" :min="64000" :max="10000000" :step="64000" :use-grouping="false" show-buttons :disabled="pending || !token || usbStreaming" />
        </template>
        <span>{{ t('camera_runtime_only') }}</span>
        <Button type="submit" icon="pi pi-check" :label="t('apply_camera')" :loading="pending" :disabled="!token || usbStreaming || !compatibleCodec || !modes.some(mode => mode.id === modeId && mode.available)" />
      </form>
      <form v-if="udp?.ready" class="camera-settings-grid" @submit.prevent="applyUdp">
        <label for="camera-udp">RTP / UDP</label>
        <ToggleSwitch input-id="camera-udp" v-model="udpEnabled" :disabled="pending || !token || usbStreaming" />
        <label for="camera-udp-port">{{ t('udp_port') }}</label>
        <InputNumber input-id="camera-udp-port" v-model="udpPort" :min="1024" :max="65534" :step="2" :use-grouping="false" show-buttons :disabled="pending || !token || usbStreaming" />
        <span>{{ t('udp_receiver') }}</span><span>{{ udp.receiver || t('unknown') }}</span>
        <span>{{ t('udp_frames') }}</span><span>{{ udp.frames_sent }} / {{ udp.oversized_frames }} / {{ udp.send_errors }}</span>
        <template v-if="udp.last_frame_age_us !== undefined">
          <span>{{ t('udp_frame_age') }}</span><span>{{ (udp.last_frame_age_us / 1000).toFixed(1) }} ms</span>
        </template>
        <template v-if="udp.last_send_duration_us !== undefined">
          <span>{{ t('udp_send_time') }}</span><span>{{ (udp.last_send_duration_us / 1000).toFixed(1) }} ms</span>
        </template>
        <Button type="submit" icon="pi pi-send" :label="t('apply_udp')" :loading="pending" :disabled="!token" />
        <a v-if="udp.enabled" :href="sdpLink" download="csi-0.sdp"><i class="pi pi-download" aria-hidden="true"></i> SDP</a>
      </form>
    </template>
  </section>
</template>