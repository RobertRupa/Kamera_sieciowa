import { expect, test } from '@playwright/test'

test('offline dashboard, preferences and failures', async ({ page }, info) => {
  await page.route('http://camera-hub.local/**', route => route.abort())
  await page.goto('/')
  await expect(page.getByRole('heading', { name: 'Camera', exact: true })).toBeVisible()
  await expect(page.getByText('No signal', { exact: true })).toBeVisible()
  await expect(page.locator('.video-surface img')).toHaveCount(0)
  await expect(page.getByText('Invalid PrimeUI License', { exact: true })).toHaveCount(0)
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true)
  await page.screenshot({ path: `test-results/camera-${info.project.name}.png`, fullPage: true })
  await page.getByRole('button', { name: 'Connect', exact: true }).click()
  await expect(page.getByRole('alert')).toContainText('Device unavailable')
  await expect(page.getByText('No signal', { exact: true })).toBeVisible()
  await page.getByRole('combobox', { name: 'Language', exact: true }).click()
  await page.getByRole('option', { name: 'Polski', exact: true }).click()
  await expect(page.getByRole('heading', { name: 'Kamera', exact: true })).toBeVisible()
  await expect(page.locator('html')).toHaveAttribute('lang', 'pl')
  await page.getByRole('combobox', { name: 'Motyw', exact: true }).click()
  await page.getByRole('option', { name: 'Ciemny', exact: true }).click()
  await expect(page.locator('html')).toHaveClass('dark')
  await page.getByRole('link', { name: 'Diagnostyka', exact: true }).click()
  await expect(page.getByRole('heading', { name: 'Diagnostyka', exact: true })).toBeVisible()
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true)
})

test('camera status separates hardware detection from active capture', async ({ page }) => {
  let camera: { width: number; height: number; fps: number } | null = null
  let cameraProbeState = 'sensor_identified'
  await page.route('**/api/v1/device/**', route => {
    const data = route.request().url().endsWith('/status')
      ? { firmware_version: 'test-fixture', uptime_seconds: 0, free_heap_bytes: 0, camera,
          camera_probe_state: cameraProbeState,
          camera_hardware: { sensor: 'OV5647', module: 'UC-439', module_revision: 'B' } }
      : { board_id: 'test-fixture', target: 'esp32p4', capabilities: { http_mjpeg: 'not_ready' } }
    return route.fulfill({ json: { data } })
  })
  await page.goto('/')
  await page.getByLabel('Device address', { exact: true }).fill('http://127.0.0.1:4173')
  await page.getByRole('button', { name: 'Connect', exact: true }).click()

  const status = page.getByRole('region', { name: 'Camera status', exact: true })
  await expect(status.getByText('Camera detected', { exact: true })).toBeVisible()
  await expect(status.getByText('UC-439 rev.B', { exact: true })).toBeVisible()
  await expect(status.getByText('OV5647', { exact: true })).toBeVisible()
  await expect(status.getByText('Sensor identified; CSI link unverified', { exact: true })).toBeVisible()
  await expect(status.getByText('No active capture', { exact: true })).toBeVisible()

  camera = { width: 1280, height: 720, fps: 30 }
  cameraProbeState = 'frame_captured'
  await page.getByRole('button', { name: 'Refresh', exact: true }).click()
  await expect(status.getByText('One CSI frame received', { exact: true })).toBeVisible()
  await expect(status.getByText('Capture active', { exact: true })).toBeVisible()
  await expect(status.getByText('1280 × 720 · 30 FPS', { exact: true })).toBeVisible()
})

test('camera diagnostics separates reported hardware from unknown detection', async ({ page }, info) => {
  await page.goto('/#/diagnostics')
  const camera = page.getByRole('region', { name: 'Camera', exact: true })
  await expect(camera.getByText('UC-439 rev.B', { exact: true })).toBeVisible()
  await expect(camera.getByText('IMX219 / OV5647', { exact: true })).toBeVisible()
  const sensorRow = camera.locator('.diagnostic-list > div').filter({ has: page.getByText('Detected sensor', { exact: true }) })
  await expect(sensorRow.locator('dd')).toHaveText('Unknown')
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true)
  await page.screenshot({ path: `test-results/camera-diagnostics-${info.project.name}.png`, fullPage: true })
  await page.getByRole('combobox', { name: 'Language', exact: true }).click()
  await page.getByRole('option', { name: 'Polski', exact: true }).click()
  await expect(page.getByText('Zgłoszony moduł', { exact: true })).toBeVisible()
  await expect(page.getByText('Wykryty sensor', { exact: true })).toBeVisible()
})

for (const sensor of ['IMX219', 'OV5647']) {
  test(`camera diagnostics preserves detected ${sensor} from API`, async ({ page }) => {
    await page.route('**/api/v1/device/**', route => {
      const data = route.request().url().endsWith('/status')
        ? { firmware_version: 'test-fixture', uptime_seconds: 0, free_heap_bytes: 0, camera: null,
            camera_hardware: { sensor, module: 'UC-439', module_revision: 'B' } }
        : { board_id: 'test-fixture', target: 'esp32p4', capabilities: { http_mjpeg: 'not_ready' } }
      return route.fulfill({ json: { data } })
    })
    await page.goto('/')
    await page.getByLabel('Device address', { exact: true }).fill('http://127.0.0.1:4173')
    await page.getByRole('button', { name: 'Connect', exact: true }).click()
    await expect(page.getByText('Connected', { exact: true })).toBeAttached()
    await expect(page.locator('.video-surface img')).toHaveCount(0)
    await page.getByRole('link', { name: 'Diagnostics', exact: true }).click()
    const camera = page.getByRole('region', { name: 'Camera', exact: true })
    const sensorRow = camera.locator('.diagnostic-list > div').filter({ has: page.getByText('Detected sensor', { exact: true }) })
    await expect(sensorRow.locator('dd')).toHaveText(sensor)
    const moduleRow = camera.locator('.diagnostic-list > div').filter({ has: page.getByText('Detected module', { exact: true }) })
    await expect(moduleRow.locator('dd')).toHaveText('UC-439 rev.B')
  })
}

test('Wi-Fi setup hides credentials when prerequisites are missing', async ({ page }, info) => {
  await page.route('**/api/v1/provisioning/status', route => route.fulfill({ json: { data: {
    wifi: 'ready', wifi_wizard: 'ready', credential_storage: 'not_ready',
    station_ip: null, provisioning_available: true,
  } } }))
  await page.goto('/#/wifi')
  await expect(page.getByRole('heading', { name: 'Wi-Fi Setup', exact: true })).toBeVisible()
  await expect(page.getByText(/Credential storage is unavailable/)).toBeVisible()
  await expect(page.getByLabel('Wi-Fi password', { exact: true })).toBeVisible()
  await expect(page.getByRole('button', { name: 'Connect', exact: true })).toBeDisabled()
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true)
  await page.screenshot({ path: `test-results/wifi-${info.project.name}.png`, fullPage: true })
})

test('Wi-Fi LAN view shows assigned IP and keeps provisioning controls on AP', async ({ page }) => {
  await page.route('**/api/v1/provisioning/status', route => route.fulfill({ json: { data: {
    wifi: 'ready', wifi_wizard: 'ready', credential_storage: 'ready',
    station_ip: '192.168.69.126', provisioning_available: false,
  } } }))
  await page.goto('/#/wifi')
  await expect(page.getByRole('link', { name: '192.168.69.126', exact: true }))
    .toHaveAttribute('href', 'http://192.168.69.126/')
  await expect(page.getByText(/changes are available only while connected/)).toBeVisible()
  await expect(page.getByRole('button', { name: 'Scan networks' })).toHaveCount(0)
  await expect(page.getByLabel('Wi-Fi password', { exact: true })).toHaveCount(0)
})

test('Wi-Fi setup scans, submits credentials, polls connection and clears password', async ({ page }) => {
  await page.route('**/api/v1/provisioning/status', route => route.fulfill({ json: { data: {
    wifi: 'ready', wifi_wizard: 'ready', credential_storage: 'ready',
    station_ip: null, provisioning_available: true,
  } } }))
  await page.route('**/api/v1/provisioning/session', route => route.fulfill({ json: {
    data: { token: 'test-session-token'.repeat(2), expires_in_seconds: 60 },
  } }))
  await page.route('**/api/v1/network/scan', route => {
    expect(route.request().headers().authorization).toContain('Bearer test-session-token')
    return route.fulfill({ status: 202, json: { data: { operation_id: 'scan-3' } } })
  })
  await page.route('**/api/v1/network/scan/3', route => route.fulfill({ json: { data: {
    operation_id: 'scan-3', state: 'complete',
    results: [{ ssid: 'test-network', rssi: -42, security: 'wpa2_psk' }],
  } } }))
  await page.route('**/api/v1/network/credentials', route => {
    expect(route.request().headers().authorization).toContain('Bearer test-session-token')
    return route.fulfill({ status: 202, json: { data: { operation_id: 'wifi-5' } } })
  })
  await page.route('**/api/v1/network/operations/wifi-5', route => route.fulfill({ json: { data: {
    operation_id: 'wifi-5', state: 'connected', station_ip: '192.168.69.126',
  } } }))
  await page.goto('/#/wifi')
  await expect(page.getByText(/stored unencrypted/)).toBeVisible()
  await page.getByRole('button', { name: 'Scan networks' }).click()
  await page.getByRole('combobox', { name: 'Select a network' }).click()
  await page.getByRole('option', { name: /test-network/ }).click()
  await expect(page.getByLabel('SSID', { exact: true })).toHaveValue('test-network')
  await page.getByLabel('Wi-Fi password', { exact: true }).fill('test-fixture-password')
  await page.getByRole('button', { name: 'Connect', exact: true }).click()
  await expect(page.getByText('Connected: wifi-5', { exact: true })).toBeVisible()
  await expect(page.getByRole('link', { name: '192.168.69.126', exact: true }))
    .toHaveAttribute('href', 'http://192.168.69.126/')
  await expect(page.getByLabel('Wi-Fi password', { exact: true })).toHaveValue('')
})