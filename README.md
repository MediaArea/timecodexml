# MediaTimecode

## Abstract

MediaTimecode XML is a structured intermediate document that can store one or many expressions of timecode. The project includes tools (mediainfo) to produce MediaTimecode XML from supported audiovisual files as well as tools to transform that XML into accessible representations such as a WebVTT subtitle document. This is a work-in-progress and a request-for-comments, please feel welcome to share thoughts and ideas.

## Goals

The following scenarios are supported:

- `av_file_with_timecode.mxf` > `timecode.xml`
- `timecode.xml` > `timecode.vtt`
- `av_file_without_timecode.mxf` + `timecode.vtt` > `av_file_with_timecode.mkv`

The following scenarios are envisioned:

- `av_file_without_timecode.[mkv|dv|mov|mxf]` > `timecode.xml`
- `av_file_without_timecode.[mkv|dv|mov|mxf]` + `timecode.vtt` > `av_file_with_timecode.[mkv|dv|mov|mxf]`

## MediaTimecode XML

This document represents one or many timecode tracks. There are two primary types:
- timecode tracks where the entire sequences of timecodes can be noted by an initial value and the information on how to increment those values upon a timeline
- timecode tracks where each value is stored indepdently, which is useful for storing non-continuous timecodes

The structure and semantics of TimecodeXML are documented in a [MediaTimecode XML Schema](MediaTimecode.xsd).

### How to make MediaTimecode XML

Using Mediainfo daily builds from 2023-01-27 or later run:

`mediainfo --ParseSpeed=1 Output=TimecodeXML file.mxf`

Note: adding `--ParseSpeed=1` means that mediainfo will read through all the timecode data of the file to represent supported timecode tracks completely in the XML output. Without `--ParseSpeed=0`, the XML output will only include encough information as can be gathered in a quick scan. The resulting XML will represent if this evaluation was full or not in the `@full` attribute.

## MediaTimecode VTT

### Introduction to MediaTimecode VTT

While the number of applications that support a presentation of a timecode stream are limited, there is an abundance of media tools that support the presentation of subtitle tracks. As such, in order to make timecode data, more accessible, MediaTimecode XML may be transformed into MediaTimecode VTT, which presents the same data as a subtitle format using the tool, `timecodexml2webvtt`.

The resulting WebVTT subtitle file can be embedded into media files or accessed in many players as a sidecar file of the media file it represents.

### How to make timecodexml2webvtt

Checkout the timecodexml repository (this one) and run `make`.

## How to convert MediaTimecode XML to VTT.

`timecodexml2webvtt tc.xml > tc.vtt`

## Recommendations for storing MediaTimecode subtitle data in an audiovisual container

The authors propose the following recommendations when muxing MediaTimecode VTT into an audiovisual container.

- Set the language code to non-lingual content. Within ISO 639-2, ISO 639-3, and [IETF BCP47](https://www.ietf.org/rfc/bcp/bcp47.txt), the code `zxx` is defined for subtitles with no linguistic content. In ffmpeg this language tag can be added to the muxer with the command `-metadata:s:s:0 language=zxx` (presuming that the MediaTimecode VTT subtitle is the first subtitle referenced, aka the `0` in the stream reference of `s:s:0`.

- Set the title of the subtitle to `MediaTimecode`. Such labelling standardizes the use of the MediaTimecode subtitle and may make it easier for other future parsers to understand that this subtitle track is used to store and make accessible timecode data. In ffmpeg, such a title can be added to the first subtitle track sent to the muxer with `-metadata:s:s:0 title="MediaTimecode"`.

## Example Workflow

Here is an example of:
- creating a MediaTimecode XML from an MXF file
- converting that XML into a subtitle file
- rewrapping the audiovisual streams of the MXF file plus the MediaTimecode VTT into a new Matroska file

```
mediainfo --ParseSpeed=1 --Output=TimecodeXML fun-movie.mxf > fun-movie.xml
timecodexml2webvtt fun-movie.xml > fun-movie.vtt
ffmpeg -i fun-movie.mxf -i fun-movie.vtt -map 0:v? -map 0:a? -map 0:s? -c copy -map 1:s? -metadata:s:s:0 language=zxx -metadata:s:s:0 title="MediaTimecode" fun-movie.mkv
```
