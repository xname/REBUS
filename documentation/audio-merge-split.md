# audio

## merge

### have

3 stereo WAV files

### want

1 six-channel WAV file

### do

```
ffmpeg -i output.wav -i input.wav -i control.wav -ac 6 \
  -filter_complex "[0:0] [1:0] amerge [tmp], [tmp] [2:0] amerge" \
  -codec pcm_f32le out.wav
```

## split

### haev

1 six-channel WAV file

### want

3 stereo WAV files

### do

```
ffmpeg -i in.wav \
  -map_channel 0.0.0:0.0 -map_channel 0.0.1:0.0 -ac 2 -acodec pcm_f32le output.wav \
  -map_channel 0.0.2:1.0 -map_channel 0.0.3:1.0 -ac 2 -acodec pcm_f32le input.wav \
  -map_channel 0.0.4:2.0 -map_channel 0.0.5:2.0 -ac 2 -acodec pcm_f32le control.wav
```
