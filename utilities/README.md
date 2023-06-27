# Utilities

## rebus-control-histogram

Generates a map of control movements
assuming they are saved as
the last two channels of a 6-channel WAV
(as recorded by the composition template).

Compile with `make` (needs libsndfile).

Using ImageMagick to convert PPM to PNG,
as applied to directory of WAVs:

```
for i in */*.wav
do
  if [ ! "$i.png" -nt "$i.wav" ]
  then
    rebus-control-histogram "$i" "$i.ppm"
    convert "$i.ppm" "${i%wav}png"
    rm "$i.ppm"
  fi
done
```
