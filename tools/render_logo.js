const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: 1180, height: 440 } });
  const svg = require('fs').readFileSync('art/logo.svg', 'utf8');
  await p.setContent(`<html><body style="margin:0;background:transparent">${svg}</body></html>`);
  await p.screenshot({ path: 'art/logo.png', omitBackground: true });
  await p.setContent(`<html><body style="margin:0;background:#101820">${svg}</body></html>`);
  await p.screenshot({ path: 'art/preview_dark.png' });
  await p.setContent(`<html><body style="margin:0;background:#cfd6dc">${svg}</body></html>`);
  await p.screenshot({ path: 'art/preview_light.png' });
  await b.close();
})();
