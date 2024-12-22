import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
	integrations: [
		starlight({
			title: 'StaticLinux App Management Tool',
			social: {
				github: 'https://github.com/staticlinux/app',
			},
            sidebar: [{
                label: 'Commands',
                autogenerate: {
                    directory: 'commands'
                }
            }]
		}),
	],
    redirects: {
        '/': '/commands/pull'
    }
});