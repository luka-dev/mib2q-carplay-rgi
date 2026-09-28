$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force 'D:\upmix\runs\listening' | Out-Null
Set-Location 'D:\upmix\tools'
python 'D:\upmix\tools\select_carplay_listening_songs.py' `
    --cache 'D:\upmix\datasets\teacher_cache4' `
    --limit 0 `
    --seed 1609 `
    --out 'D:\upmix\runs\listening\passage_screen_all.json' `
    *> 'D:\upmix\runs\listening\passage_screen_all.log'
