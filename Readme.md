# suidless su
`su` command replacement. No suid bit required.

## Build
build using meson
```sh
meson setup build
ninja -C build
ninja -C build install
```

## Dependencies
* nothing

## Usage
1- start daemon as root (or with service)
```sh
su --daemon
```
2- use it
```sh
su
```

