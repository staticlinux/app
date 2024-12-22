---
title: app pull
---

## Description
Download an app from staticlinux.org.

## Usage
```
app pull [OPTIONS] NAME[/PATH][:VERSION]
```

## Options

## Example
```
$ app pull bash/bash:5.2.37
Pulling from http://apps.staticlinux.org/bash/5.2.37/amd64/bash
Pull completed
Size: 1563192
MD5: f10b404d8f471ec1ac7f26f838f1d259
Save to: ~/.staticlinux/bash/bash
Add symbol link: ~/.staticlinux/bin/bash

$ ~/.staticlinux/bin/bash --version
GNU bash, version 5.2.32(1)-release (x86_64-pc-linux-gnu)
Copyright (C) 2022 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>

This is free software; you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
```