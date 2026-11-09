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
  await page.goto('/#/wifi')
  await expect(page.getByRole('heading', { name: 'Wi-Fi Setup', exact: true })).toBeVisible()
  await expect(page.getByText('Device not connected', { exact: true })).toBeVisible()
  await expect(page.getByLabel('Wi-Fi password', { exact: true })).toHaveCount(0)
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true)
  await page.screenshot({ path: `test-results/wifi-${info.project.name}.png`, fullPage: true })
})

test('Wi-Fi setup reports queued operation and clears password', async ({ page }) => {
  await page.route('**/api/v1/device/**', route => route.fulfill({ json: { data:
    route.request().url().endsWith('/status')
      ? { firmware_version: 'test-fixture', uptime_seconds: 0, free_heap_bytes: 0, camera: null }
      : { board_id: 'test-fixture', target: 'esp32p4', capabilities: {
          wifi: 'ready', wifi_wizard: 'ready', credential_storage: 'ready', http_mjpeg: 'not_ready' } }
  } }))
  await page.route('**/api/v1/provisioning/session', route => route.fulfill({ json: {
    data: { token: 'test-session-token'.repeat(2), expires_in_seconds: 60 },
  } }))
  await page.route('**/api/v1/network/credentials', route => {
    expect(route.request().headers().authorization).toContain('Bearer test-session-token')
    return route.fulfill({ status: 202, json: { data: { operation_id: 'op-browser-test' } } })
  })
  await page.goto('/')
  await page.getByLabel('Device address', { exact: true }).fill('http://127.0.0.1:4173')
  await page.getByRole('button', { name: 'Connect', exact: true }).click()
  await expect(page.getByText('Connected', { exact: true })).toBeAttached()
  await page.getByRole('link', { name: 'Wi-Fi Setup', exact: true }).click()
  await expect(page.getByText(/stored unencrypted/)).toBeVisible()
  await page.getByLabel('SSID', { exact: true }).fill('test-network')
  await page.getByLabel('Wi-Fi password', { exact: true }).fill('test-fixture-password')
  await page.getByRole('button', { name: 'Connect', exact: true }).click()
  await expect(page.getByText('Connection queued: op-browser-test', { exact: true })).toBeVisible()
  await expect(page.getByLabel('Wi-Fi password', { exact: true })).toHaveValue('')
})