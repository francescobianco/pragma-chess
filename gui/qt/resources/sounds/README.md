# Sounds

`move.wav`, a piece set down on a wooden board, is `impactWood_light_000`
from Kenney's *Impact Sounds* (<https://kenney.nl/assets/impact-sounds>),
released under Creative Commons Zero (CC0, public domain). Converted with:

```bash
ffmpeg -i impactWood_light_000.ogg -ac 1 -ar 44100 \
  -af "atrim=0:0.16,afade=t=out:st=0.12:d=0.04,volume=0.8" -c:a pcm_s16le move.wav
```

WAV, because every system's own player reads it (PlaySound on Windows only
reads WAV).
