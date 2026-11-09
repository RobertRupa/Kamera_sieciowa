import { afterEach, describe, expect, it, vi } from 'vitest'
import { createProvisioningSession, getProvisioningStatus, getWifiOperation, getWifiScan, startWifiScan, submitWifi, wifiCredentialsSchema } from './provisioning'

afterEach(() => vi.unstubAllGlobals())

describe('Wi-Fi setup boundary', () => {
  it('validates SSID byte length and password bounds', () => {
    expect(wifiCredentialsSchema.safeParse({ ssid: 'a'.repeat(32), password: 'a'.repeat(8) }).success).toBe(true)
    expect(wifiCredentialsSchema.safeParse({ ssid: 'ą'.repeat(17), password: 'a'.repeat(8) }).success).toBe(false)
    expect(wifiCredentialsSchema.safeParse({ ssid: 'a\0b', password: 'a'.repeat(8) }).success).toBe(false)
    expect(wifiCredentialsSchema.safeParse({ ssid: 'test', password: 'short' }).success).toBe(false)
  })
  it('uses a scoped token and returns only an operation identifier', async () => {
    const token = 'a'.repeat(32)
    const fetchMock = vi.fn().mockResolvedValue(new Response(JSON.stringify({ data: { operation_id: 'wifi-1' } })))
    vi.stubGlobal('fetch', fetchMock)
    await expect(submitWifi('https://camera.local', { ssid: 'test', password: 'a'.repeat(8) }, token)).resolves.toBe('wifi-1')
    expect(fetchMock.mock.calls[0]?.[1]?.headers.Authorization).toBe(`Bearer ${token}`)
    expect(fetchMock.mock.calls[0]?.[1]?.credentials).toBe('omit')
  })
  it('creates an in-memory session for provisioning requests', async () => {
    const token = 'b'.repeat(32)
    const fetchMock = vi.fn().mockResolvedValue(new Response(JSON.stringify({ data: { token, expires_in_seconds: 60 } })))
    vi.stubGlobal('fetch', fetchMock)
    await expect(createProvisioningSession('http://camera.local')).resolves.toEqual({ token, expires_in_seconds: 60 })
    expect(fetchMock.mock.calls[0]?.[1]?.method).toBe('POST')
  })
  it('reads AP-local readiness without requesting device capability APIs', async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response(JSON.stringify({ data: {
      wifi: 'ready', wifi_wizard: 'ready', credential_storage: 'not_ready',
      station_ip: '192.168.69.126', provisioning_available: false,
    } })))
    vi.stubGlobal('fetch', fetchMock)
    await expect(getProvisioningStatus('http://192.168.4.1')).resolves.toEqual({
      wifi: 'ready', wifi_wizard: 'ready', credential_storage: 'not_ready',
      station_ip: '192.168.69.126', provisioning_available: false,
    })
    expect(fetchMock.mock.calls[0]?.[0]).toBe('http://192.168.4.1/api/v1/provisioning/status')
  })
  it('polls the versioned connection operation endpoint with the scoped token', async () => {
    const token = 'e'.repeat(32)
    const fetchMock = vi.fn().mockResolvedValue(new Response(JSON.stringify({ data: {
      operation_id: 'wifi-4', state: 'connecting', station_ip: null,
    } })))
    vi.stubGlobal('fetch', fetchMock)
    await expect(getWifiOperation('http://camera.local', token, 'wifi-4')).resolves.toMatchObject({ state: 'connecting' })
    expect(fetchMock.mock.calls[0]?.[0]).toBe('http://camera.local/api/v1/network/operations/wifi-4')
    expect(fetchMock.mock.calls[0]?.[1]?.headers.Authorization).toBe(`Bearer ${token}`)
  })
  it('starts and polls a scan using the scoped token', async () => {
    const token = 'c'.repeat(32)
    const fetchMock = vi.fn()
      .mockResolvedValueOnce(new Response(JSON.stringify({ data: { operation_id: 'scan-7' } })))
      .mockResolvedValueOnce(new Response(JSON.stringify({ data: {
        operation_id: 'scan-7', state: 'complete', results: [{ ssid: 'lab-network', rssi: -48, security: 'wpa2_psk' }],
      } })))
    vi.stubGlobal('fetch', fetchMock)
    const operationId = await startWifiScan('http://camera.local', token)
    const scan = await getWifiScan('http://camera.local', token, operationId)
    expect(scan.results[0]?.ssid).toBe('lab-network')
    expect(fetchMock.mock.calls[0]?.[1]?.headers.Authorization).toBe(`Bearer ${token}`)
    expect(fetchMock.mock.calls[1]?.[0]).toBe('http://camera.local/api/v1/network/scan/7')
  })
  it('does not submit credentials when the provisioning session is denied', async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response('', { status: 403 }))
    vi.stubGlobal('fetch', fetchMock)
    await expect(submitWifi('https://camera.local', { ssid: 'test', password: 'a'.repeat(8) }, 'd'.repeat(32))).rejects.toThrow('unauthorized')
    expect(fetchMock).toHaveBeenCalledTimes(1)
  })
})