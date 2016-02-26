[![Build status](https://ci.appveyor.com/api/projects/status/ctf2wj6im4d05c0s?svg=true)](https://ci.appveyor.com/project/olaolsso/pixiple)

# Pixiple

## What's Pixiple?

Pixiple is a Windows application that searches your files for images that are similar in pixel and metadata content and presents you with a sorted list of similar image pairs to compare.

## Screenshot

![Screenshot](screenshot.jpg)

## Download

Download the executable from [Releases](https://github.com/olaolsso/pixiple/releases).

## Features

- Fast, multi-threaded processing, though even with a fast SSD, you will likely be IO-bound.
- Resizable, DPI-aware UI.

## Misfeatures

- Simple, idiosyncratic UI with rough edges.
- Support for image file formats supported by Windows Imaging Component (PNG, JPEG, GIF, TIFF, BMP) only.
- Only one folder (and all folders below this folder, recursively) is scanned at a time.
- No installer.

## Requirements

Windows 7 (64-bit). May work with Windows Vista and Windows 10.

## What's similar?

Pixiple will easily detect images that are identical, have identical pixel content, are uniformly resized, flipped, rotated (90, 180, 270 degrees) or have minor differences in pixel content.

Pixiple is less well able to detect similar images with significant changes to pixel content (cropping or change of brightness, contrast, saturation, etc).

Pixiple ignores file metadata (name, path, size, date, format) when detecting similarity.
