[![Build status](https://ci.appveyor.com/api/projects/status/ctf2wj6im4d05c0s?svg=true)](https://ci.appveyor.com/project/olaolsso/pixiple)

# Pixiple

## What's Pixiple?

Pixiple is a Windows application that searches your files for images that are similar in pixel and metadata content and presents you with a sorted list of similar image pairs to compare.

## Screenshot

![Screenshot](screenshot.jpg)

## Download

Download the executable from [Releases](https://github.com/olaolsso/pixiple/releases).

## Features

- Uses pixel and metadata content (dates, location, camera) to find related images.
- Fast, multi-threaded processing.
- Resizable, DPI-aware UI.

## Misfeatures

- Simple, idiosyncratic UI with rough edges.
- Support for image file formats supported by Windows Imaging Component only (PNG, JPEG, GIF, TIFF, BMP).
- No installer.

## Requirements

Windows Vista or later.

## What's similar?

Pixiple will easily detect images that are identical, have identical pixel content, are uniformly resized, flipped, rotated (90, 180, 270 degrees) or have minor differences in pixel content.

Pixiple is less well able to detect similar images with significant changes to pixel content (cropping or change of brightness, contrast, saturation, etc).

Pixiple ignores file metadata (name, path, size, date, format) when detecting similarity.
