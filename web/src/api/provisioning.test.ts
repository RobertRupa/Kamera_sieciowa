import { afterEach, describe, expect, it, vi } from 'vitest'
import { submitWifi, wifiCredentialsSchema } from './provisioning'

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
    const fetchMock = vi.fn()
      .mockResolvedValueOnce(new Response(JSON.stringify({ data: { token, expires_in_seconds: 60 } })))
      .mockResolvedValueOnce(new Response(JSON.stringify({ data: { operation_id: 'op-test' } })))
    vi.stubGlobal('fetch', fetchMock)
    await expect(submitWifi('https://camera.local', { ssid: 'test', password: 'a'.repeat(8) })).resolves.toBe('op-test')
    expect(fetchMock.mock.calls[1]?.[1]?.headers.Authorization).toBe(`Bearer ${token}`)
    expect(fetchMock.mock.calls[1]?.[1]?.credentials).toBe('omit')
  })
  it('does not submit credentials when provisioning session is denied', async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response('', { status: 403 }))
    vi.stubGlobal('fetch', fetchMock)
    await expect(submitWifi('https://camera.local', { ssid: 'test', password: 'a'.repeat(8) })).rejects.toThrow('unauthorized')
    expect(fetchMock).toHaveBeenCalledTimes(1)
  })
})