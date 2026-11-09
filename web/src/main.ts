import { createApp } from 'vue'
import { createPinia } from 'pinia'
import { createI18n } from 'vue-i18n'
import { createRouter, createWebHashHistory } from 'vue-router'
import PrimeVue from 'primevue/config'
import Aura from '@primeuix/themes/aura'
import '@fontsource/ibm-plex-sans/latin-400.css'
import '@fontsource/ibm-plex-sans/latin-ext-400.css'
import '@fontsource/ibm-plex-sans/latin-500.css'
import '@fontsource/ibm-plex-sans/latin-ext-500.css'
import '@fontsource/ibm-plex-sans/latin-600.css'
import '@fontsource/ibm-plex-sans/latin-ext-600.css'
import 'primeicons/primeicons.css'
import './dashboard.css'
import en from './locales/en.json'
import pl from './locales/pl.json'
import App from './App.vue'
import CameraView from './views/CameraView.vue'
import DiagnosticsView from './views/DiagnosticsView.vue'
import WifiView from './views/WifiView.vue'

const router = createRouter({
	history: createWebHashHistory(),
	routes: [
		{ path: '/', component: CameraView },
		{ path: '/diagnostics', component: DiagnosticsView },
		{ path: '/wifi', component: WifiView },
	],
})

createApp(App)
	.use(createPinia())
	.use(createI18n({ legacy: false, locale: 'en', fallbackLocale: 'en', messages: { en, pl } }))
	.use(router)
	.use(PrimeVue, { theme: { preset: Aura, options: { darkModeSelector: '.dark' } } })
	.mount('#app')
