(() => {
  'use strict';
  window.initSiteExperience = function () {
    const audio=document.getElementById('site-audio');
    const artist=document.getElementById('artist-shot');
    const status=document.getElementById('artist-audio-status');
    const soundButton=document.getElementById('hero-sound-toggle');
    const gallery=document.querySelector('.film-gallery');
    const cards=[...gallery.querySelectorAll('.card')];
    const zones=[...artist.querySelectorAll('[data-persona]')];
    const tracks={skazi:{name:'SKAZI · JUNAM',src:'audio/skazi-junam.mp3'},asher:{name:'ASHER SWISSA · IMANI',src:'audio/asher-imani.mp3'}};
    let enabled=false,target=null,token=0;
    audio.volume=.75;
    function stop(){++token;audio.pause();target=null;artist.classList.remove('is-playing');zones.forEach(z=>z.setAttribute('aria-pressed','false'));}
    function setEnabled(value){
      enabled=value;soundButton.setAttribute('aria-pressed',String(value));
      soundButton.textContent=value?'SOUND ON · MUTE':'SOUND OFF · ENABLE';
      if(!value){stop();status.textContent='Artist previews muted.';}
    }
    async function play(persona){
      if(!enabled||document.hidden)return;
      if(target===persona&&!audio.paused)return;
      stop();target=persona;const request=++token;
      const track=tracks[persona];
      const url=new URL(track.src,document.baseURI).href;
      if(audio.src!==url)audio.src=url;
      else {try{audio.currentTime=0;}catch{}}
      try{
        await audio.play();
        if(request!==token)return;
        artist.classList.add('is-playing');
        zones.find(z=>z.dataset.persona===persona)?.setAttribute('aria-pressed','true');
        status.textContent=`Playing ${track.name}. Use Sound On to mute.`;
      }catch(error){
        if(request!==token)return;
        setEnabled(false);
        status.textContent=error.name==='NotAllowedError'?'Playback was blocked. Activate Enable sound to retry.':'This audio preview is unavailable.';
      }
    }
    soundButton.addEventListener('click',()=>{
      setEnabled(!enabled);
      if(enabled)play(artist.dataset.active||'skazi');
    });
    zones.forEach(zone=>{
      const persona=zone.dataset.persona;
      zone.setAttribute('aria-pressed','false');
      zone.addEventListener('pointerenter',event=>{
        artist.dataset.active=persona;
        if(event.pointerType!=='touch')play(persona);
      });
      zone.addEventListener('pointerleave',()=>{
        artist.dataset.active='';
        if(target===persona)stop();
      });
      // Focus highlights the identity without unexpectedly starting audio.
      zone.addEventListener('focus',()=>{artist.dataset.active=persona;});
      zone.addEventListener('blur',()=>{artist.dataset.active='';});
      zone.addEventListener('click',()=>{
        if(target===persona&&!audio.paused){stop();return;}
        setEnabled(true);artist.dataset.active=persona;play(persona);
      });
    });
    document.querySelector('.hero-wave').addEventListener('click',()=>{
      stop();document.getElementById('plugins').focus({preventScroll:true});
    });
    cards.forEach(card=>{
      const reduced=matchMedia('(prefers-reduced-motion: reduce)');
      card.addEventListener('pointermove',event=>{
        if(gallery.dataset.view!=='grid'||reduced.matches||event.pointerType==='touch')return;
        const bounds=card.getBoundingClientRect();
        const x=(event.clientX-bounds.left)/bounds.width, y=(event.clientY-bounds.top)/bounds.height;
        card.style.setProperty('--tilt-x',`${(0.5-y)*5}deg`);
        card.style.setProperty('--tilt-y',`${(x-0.5)*6}deg`);
        card.style.setProperty('--glow-x',`${x*100}%`);
        card.style.setProperty('--glow-y',`${y*100}%`);
      });
      card.addEventListener('pointerleave',()=>{card.style.setProperty('--tilt-x','0deg');card.style.setProperty('--tilt-y','0deg');});
    });
    audio.addEventListener('ended',()=>{stop();status.textContent='Artist preview finished.';});
    document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});

    const overlay=document.getElementById('site-intro');
    if(document.documentElement.classList.contains('intro-pending')){
      const main=document.getElementById('main');
      const navigation=document.querySelector('nav');
      const reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;
      let finished=false,started=false;
      overlay.hidden=false;main.inert=true;if(navigation)navigation.inert=true;
      const soft=document.documentElement.classList.contains('intro-soft');
      const cleanup=()=>{if(finished)return;finished=true;document.documentElement.classList.remove('intro-pending','intro-soft');overlay.hidden=true;main.inert=false;if(navigation)navigation.inert=false;};
      const finish=()=>{
        if(started||finished)return;started=true;
        const duration=reduced?180:soft?650:900;
        const heroLogo=document.querySelector('.hero-logo');
        heroLogo.animate([{opacity:0},{opacity:1}],{duration,easing:'ease-in-out',fill:'forwards'});
        overlay.animate([{opacity:1},{opacity:0}],{duration,easing:'ease-in-out',fill:'forwards'}).finished.then(cleanup,cleanup);
      };
      overlay.querySelector('.intro-skip').addEventListener('click',cleanup);
      setTimeout(finish,reduced?150:soft?300:1500);
      setTimeout(cleanup,4300);
    }
    return {stop};
  };
})();
