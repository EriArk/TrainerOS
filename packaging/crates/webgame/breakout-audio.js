// SPDX-License-Identifier: GPL-3.0-or-later
// Small original procedural cues. No samples, Flash, network or audio downloads.
(function () {
  var audio;
  function activate() {
    if (!audio) audio = new (window.AudioContext || window.webkitAudioContext)();
    if (audio.state === 'suspended') audio.resume();
  }
  document.addEventListener('keydown', activate);
  document.addEventListener('pointerdown', activate);
  window.soundManager = { play: function (id) {
    if (!audio || audio.state !== 'running') return;
    var oscillator = audio.createOscillator(), gain = audio.createGain();
    var notes = {brick:660, paddle:440, loselife:180, gameover:110, levelup:880};
    var now = audio.currentTime, duration = id === 'brick' || id === 'paddle' ? .07 : .24;
    oscillator.type = 'triangle'; oscillator.frequency.value = notes[id] || 440;
    gain.gain.setValueAtTime(.10,now); gain.gain.exponentialRampToValueAtTime(.001,now+duration);
    oscillator.connect(gain); gain.connect(audio.destination); oscillator.start(now); oscillator.stop(now+duration);
  }};
  Game.loadSounds = function () {};
})();
