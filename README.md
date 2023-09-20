# mediapick

Mediapick quickly caches a list of media files from STDIN so that nsxiv (an image viewer) can view video thumbnails.
Marking those files in nsxiv will return them in STDOUT, separated by `<d>`.
This is useful for doing arbitrary actions on the marked files, such as deleting, moving, copying...

I will also be making a key handler for nsxiv to open these files in external viewers, so videos can actually be played.

## Compiling

```
$ make
```

## Installing

```
# make install
```

## Usage
```
Usage: mediapick [options]

Options:
  -d <d>          set input and output deliminator (default \n)
  -i <d>          set input deliminator
  -o <d>          set output deliminator
  -c <cachedir>   set cache folder (default $HOME/.cache/$0)
  -C              prompt to delete cachedir
  -h              print help
```
View everything in a dir:
```sh
find . -maxdepth 1 -type f | mediapick
```

Move stupid looking photos to the bin:
```sh
find pics/smartphotos | mediapick | while read file; do
    printf "Delete '$file'? (Y/n)"
    [ "$(read /dev/tty)" = "n" ] && continue
    rm -v "$file"
done
```

Handle files with `\n`
```sh
find folderwithmaliciouslynamedmediafiles/vids -print0 | mediapick -0
```

Use [lf](https://github.com/gokcehan/lf) to select a list of files, put the cached images in `~/.cache/lf`, ensure the delimiter is `\n` because shell variables cannot hold the null character, mark them with nsxiv, and re-select the marked files in `lf` to do actions on:
```sh
markList=$(echo "$fs" | mediapick -c "$HOME/.cache/lf" -d '\n')
if [ -n "$markList" ]; then
    lf -remote "send $id unselect"
    echo "$markList" | while read file; do
        lf -remote "send $id toggle \"$file\""
    done
fi
```
\(This is my main use for mediapick\)
