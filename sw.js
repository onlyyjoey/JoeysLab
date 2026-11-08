/* JoeysLab offline cache. Bump VERSION whenever index.html or samples change. */
const VERSION='joeyslab-v1';
const CORE=['./','index.html','manifest.webmanifest','icon.svg','icon-192.png','icon-512.png'];
const SAMPLES=[
  'samples/808-bird.wav',
  'samples/808-eagle.wav',
  'samples/808-ian.wav',
  'samples/808-pumpit.wav',
  'samples/bass-aloneagain.wav',
  'samples/bass-electric.wav',
  'samples/bass-guitar.wav',
  'samples/bass-hateme.wav',
  'samples/bass-kitty.wav',
  'samples/bass-minimoog.wav',
  'samples/clap-arella.wav',
  'samples/clap-finewine.wav',
  'samples/clap-idonotexist.wav',
  'samples/clap-sincity.wav',
  'samples/fx-crow.wav',
  'samples/fx-jack.wav',
  'samples/hat-dirty.wav',
  'samples/hat-fade.wav',
  'samples/hat-nonono.wav',
  'samples/hat-parking.wav',
  'samples/hat-scalp.wav',
  'samples/hat-shakeit.wav',
  'samples/hat-swag.wav',
  'samples/hat-tyler.wav',
  'samples/kick-555.wav',
  'samples/kick-cross.wav',
  'samples/kick-deep.wav',
  'samples/kick-once.wav',
  'samples/kick-smoke.wav',
  'samples/kick-tyshii.wav',
  'samples/kick-xxx.wav',
  'samples/open-novacane.wav',
  'samples/open-psshhh.wav',
  'samples/perc-bing.wav',
  'samples/perc-chopitup.wav',
  'samples/perc-filler.wav',
  'samples/perc-hype.wav',
  'samples/perc-sense.wav',
  'samples/perc-shoot.wav',
  'samples/perc-us.wav',
  'samples/rim-layer.wav',
  'samples/rim-mer.wav',
  'samples/sine-mikedean.wav',
  'samples/snare-1000lbs.wav',
  'samples/snare-300lbs.wav',
  'samples/snare-beeper.wav',
  'samples/snare-brokey.wav',
  'samples/snare-bussthru.wav',
  'samples/snare-classic.wav',
  'samples/snare-famous.wav',
  'samples/snare-impala.wav',
  'samples/snare-jokey.wav',
  'samples/snare-layerthis.wav',
  'samples/snare-neck.wav',
  'samples/snare-newer.wav',
  'samples/snare-petrol.wav',
  'samples/snare-pimpala.wav',
  'samples/snare-sanity.wav',
  'samples/snare-sexy.wav',
  'samples/tom-backup.wav',
  'samples/tom-lijk.wav',
  'samples/tom-sure.wav',
  'samples/vox-ahh.wav',
  'samples/vox-always.wav',
  'samples/vox-chant.wav',
  'samples/vox-high.wav',
  'samples/vox-oh.wav'
];
self.addEventListener('install',e=>{e.waitUntil(caches.open(VERSION).then(c=>c.addAll(CORE).then(()=>c.addAll(SAMPLES).catch(()=>{}))));self.skipWaiting();});
self.addEventListener('activate',e=>{e.waitUntil(caches.keys().then(ks=>Promise.all(ks.filter(k=>k!==VERSION).map(k=>caches.delete(k)))).then(()=>self.clients.claim()));});
self.addEventListener('fetch',e=>{
  const req=e.request;if(req.method!=='GET')return;const url=new URL(req.url);
  if(url.origin===location.origin&&url.pathname.includes('/samples/')){
    e.respondWith(caches.match(req).then(hit=>hit||fetch(req).then(r=>{const cp=r.clone();caches.open(VERSION).then(c=>c.put(req,cp));return r;})));return;}
  if(url.origin===location.origin){
    /* app files: try the network first so updates show up, fall back to the cache offline */
    e.respondWith(fetch(req).then(r=>{const cp=r.clone();caches.open(VERSION).then(c=>c.put(req,cp));return r;}).catch(()=>caches.match(req).then(h=>h||caches.match('index.html'))));return;}
  /* fonts and the mp3 encoder: cache after first use */
  e.respondWith(caches.match(req).then(hit=>hit||fetch(req).then(r=>{if(r.ok||r.type==='opaque'){const cp=r.clone();caches.open(VERSION).then(c=>c.put(req,cp));}return r;})));
});
