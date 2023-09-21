# mediapick

Mediapick quickly caches a list of media files from stdin so that [nsxiv](https://codeberg.org/nsxiv/nsxiv) can view video thumbnails.
Marking those files in nsxiv will return them in stdout, separated by `<d>`. This is useful for scripting certain actions on the marked files, like deleting, moving, etc.

I will also be making a key handler for nsxiv to open these files in external viewers, so videos can actually be played.

## building

depends on [libffmpegthumbnailer](https://github.com/dirkvdb/ffmpegthumbnailer)

### compile:
```
$ make
```

### install:
```
# make install
```

## usage
```
Usage: mediapick [options]

Options:
-D <cachedir>   set cache folder (default $HOME/.cache/$0)
-d <d>          set input and output deliminator (default \n)
-i <d>          set input deliminator
-o <d>          set output deliminator
-n <c|v>        skip caching or skip image viewing
-C              prompt to delete cachedir
-v              verbose
-h              print help
```
## examples

View everything in a dir:
```sh
find . -maxdepth 1 -type f | mediapick
```

Move stupid photos to the bin:
```sh
find pics/smartphotos | mediapick | xargs -d '\n' -I{} mv {} pics/stupidphotos/
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
\(This is my main usecase\)

### GNU Parallel

With the `-n` flag, you can skip the image viewing function and only cache files. This allows you to cache everything in parallel, provided you can divide the list of files up appropriately.
Dividing can be done with `split`, but is a bit clunky to use compared to `parallel`. Here's an example:
```sh
#!/bin/sh
delim="$1"
tmp=$(mktemp)
cat > "$tmp" # takes stdin
i=$(tr -dc "$delim" <"$tmp" | wc -c) # gets num of files
jobs=$(nproc) # get num of processors
num=$((i / jobs)) # set num of files per process
parallel --delimiter "$delim" --pipe --recend "$delim" -N $num "mediapick -d '$delim' -nv" <"$tmp"
mediapick -d "$delim" -nc <"$tmp"
rm "$tmp"
```
and do something like `find . -print0 | script.sh '\0'`.
