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
    rm -v "$file"
done
```

Handle files with `\n`
```sh
find folderwithmaliciouslynamedmediafiles/vids -print0 | mediapick -0
```

Use [a terminal file manager](https://github.com/gokcehan/lf) to select a list of files, put the cached images in `~/.cache/lf`, ensure the delimiter is `\n` because shell variables cannot hold the null character, mark them with nsxiv, and re-select the marked files in `lf` to do actions on:
```sh
fileList=$(mktemp)
markList=$(mktemp)
print0=false
recurse=false
if [ -z "$fs" ]; then
	if [ $recurse = true ]; then
		find . -type f -print > "$fileList"
	else
		find . -maxdepth 1 -type f -print > "$fileList"
	fi
else
	echo "$fs" > "$fileList"
fi
mediapick -c "$HOME/.cache/lf" -d '\n' <"$fileList" >"$markList"
lf -remote "send $id unselect"
xargs -I{} -d '\n' lf -remote "send $id toggle \"{}\"" <"$markList"
rm "$fileList" "$markList"
```
\(This is my main use for mediapick\)
