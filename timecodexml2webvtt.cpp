/* Copyright (c) MediaArea.net SARL. All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "tfsxml.h"
#include "TimeCode.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

struct stream_struct
{
    tfsxml_string   xml_handle;
    tfsxml_string   n;
    string          id;
    TimeCode        timecode;
    long long       frame_count;
};

void AddTimeStamp(string& content, uint64_t num, uint64_t den)
{
    auto before_comma = num / den;
    auto after_comma = num % den;
    after_comma = (after_comma * 1000 + den / 2) / den;
    auto H10 = before_comma / 36000;
    before_comma = before_comma % 36000;
    content += to_string(H10);
    auto H01 = (uint8_t)(before_comma / 3600);
    before_comma = before_comma % 3600;
    content += '0' + H01;
    content += ':';
    auto M10 = (uint8_t)(before_comma / 600);
    before_comma = before_comma % 600;
    content += '0' + M10;
    auto M01 = (uint8_t)(before_comma / 60);
    before_comma = before_comma % 60;
    content += '0' + M01;
    content += ':';
    auto S10 = (uint8_t)(before_comma / 10);
    before_comma = before_comma % 10;
    content += '0' + S10;
    auto S01 = (uint8_t)(before_comma);
    content += '0' + S01;
    content += '.';
    auto m100 = (uint8_t)(after_comma / 100);
    after_comma = after_comma % 100;
    content += '0' + m100;
    auto m010 = (uint8_t)(after_comma / 10);
    after_comma = after_comma % 10;
    content += '0' + m010;
    auto m000 = (uint8_t)after_comma;
    content += '0' + m000;
}

int main(int argc, char* argv[]) 
{
    if (argc < 2 || argc > 3) {
        cout <<
            "Usage: \n"
            << argv[0] << " file_name [track_index]\n"
            " file_name: Timecode XML file from MediaInfo\n"
            " track_index: 0-based track index for outputting only 1 track\n"
            ;
        return 1;
    }
    size_t track_index = argc > 2 ? stoul(argv[2]) : (size_t) - 1;
    ifstream input_file(argv[1], ios_base::in | ios_base::ate | ios_base::binary);
    auto input_size = input_file.tellg();
    if (input_size <= 0 || (size_t)input_size > numeric_limits<size_t>::max()) {
        cerr << "Error: input file too big\n";
        return 1;
    }
    input_file.seekg(0);
    auto input = new char[input_size];
    if (input_file.read(input, input_size).fail()) {
        cerr << "Error: can not read the file in full\n";
        return 1;
    }
    input_file.close();

    tfsxml_string xml_handle, n, v;
    if (tfsxml_init(&xml_handle, input, (int)input_size)) {
        cerr << "Error: issue when parsing the XML input file\n";
        return 1;
    }
    uint64_t time_stamp_inc = 0;
    uint64_t time_stamp_den = 0;
    uint64_t time_stamp_num = 0;
    vector<stream_struct> streams;
    string output("WEBVTT\n");
    size_t stream_pos = 0;
    while (!tfsxml_next(&xml_handle, &n)) {
        if (!tfsxml_strcmp_charp(n, "timecode_streams")) {
            tfsxml_enter(&xml_handle);
            while (!tfsxml_next(&xml_handle, &n)) {
                if (!tfsxml_strcmp_charp(n, "timecode_stream")) {
                    if (track_index != -1 && track_index != stream_pos++) {
                        continue;
                    }
                    output += "\nNOTE";
                    stream_struct stream;
                    while (!tfsxml_attr(&xml_handle, &n, &v)) {
                        output += ' ';
                        output += tfsxml_decode(n);
                        output += '=';
                        auto value = tfsxml_decode(v);
                        output += value;
                        if (!tfsxml_strcmp_charp(n, "frame_count")) {
                            stream.frame_count = atoll(value.c_str());
                        }
                        if (!tfsxml_strcmp_charp(n, "frame_rate")) {
                            size_t new_frame_rate_div;
                            auto new_frame_rate_num = stoull(value, &new_frame_rate_div);
                            unsigned long long new_frame_rate_den;
                            if (new_frame_rate_div < value.size() && value[new_frame_rate_div] == '/') {
                                new_frame_rate_div++;
                                new_frame_rate_den = stoull(value.substr(new_frame_rate_div));
                            }
                            else {
                                new_frame_rate_den = 1;
                            }
                            if (!time_stamp_inc && !time_stamp_den) {
                                time_stamp_den = new_frame_rate_num;
                                time_stamp_inc = new_frame_rate_den;
                            }
                            else if (new_frame_rate_num != time_stamp_den || new_frame_rate_den != time_stamp_inc)
                            {
                                cerr << "Error: issue when parsing the frame_rate attribute\n";
                                return 1;
                            }
                            auto FramesMax = (new_frame_rate_num + new_frame_rate_den - 1) / new_frame_rate_den;
                            if (!FramesMax) {
                                cerr << "Error: issue when parsing the frame_rate attribute\n";
                                return 1;
                            }
                            FramesMax--;
                            if (FramesMax >= numeric_limits<uint32_t>::max()) {
                                cerr << "Error: issue when parsing the frame_rate attribute\n";
                                return 1;
                            }
                            stream.timecode.SetFramesMax((uint32_t)FramesMax);
                        }
                        if (!tfsxml_strcmp_charp(n, "id")) {
                            if (track_index == -1) {
                                stream.id = value;
                            }
                        }
                        if (!tfsxml_strcmp_charp(n, "start_tc")) {
                            stream.timecode.FromString(value);
                        }
                    }
                    if (!stream.timecode.GetIsValid()) {
                        stream.xml_handle = xml_handle;
                        if (tfsxml_enter(&stream.xml_handle)) {
                            cerr << "Error: issue when parsing the XML input file\n";
                            return 1;
                        }
                        if (tfsxml_next(&stream.xml_handle, &stream.n)) {
                            cerr << "Error: issue when parsing the XML input file\n";
                            return 1;
                        }
                    }
                    if (track_index == -1) {
                        if (stream.id.size() < 40) {
                            stream.id.insert(stream.id.begin(), 40 - stream.id.size(), ' ');
                        }
                    }
                    stream.id.insert(stream.id.begin(), 1, '\n');
                    if (track_index == -1) {
                        stream.id += ": ";
                    }
                    streams.push_back(stream);
                }
            }
        }
    }
    if (!time_stamp_inc || !time_stamp_den) {
        cerr << "Error: frame rate is missing or not same for all tracks\n";
        return 1;
    }

    output += '\n';
    auto active_stream_count = streams.size();
    while (active_stream_count) {
        output += '\n';
        AddTimeStamp(output, time_stamp_num, time_stamp_den);
        time_stamp_num += time_stamp_inc;
        output += " --> ";
        AddTimeStamp(output, time_stamp_num, time_stamp_den);
        for (auto& stream : streams) {
            output += stream.id;
            if (stream.timecode.GetIsValid()) {
                if (stream.frame_count) {
                    output += stream.timecode.ToString();
                    stream.timecode++;
                    stream.frame_count--;
                    if (!stream.frame_count) {
                        active_stream_count--;
                    }
                }
            }
            else if (stream.n.len) {
                if (!tfsxml_strcmp_charp(stream.n, "tc")) {
                    while (!tfsxml_attr(&stream.xml_handle, &stream.n, &v)) {
                        if (!tfsxml_strcmp_charp(stream.n, "v")) {
                            tfsxml_decode(output, v);
                        }
                    }
                }
                if (tfsxml_next(&stream.xml_handle, &stream.n)) {
                    active_stream_count--;
                }
            }
        }
        output += '\n';
    }
    delete[] input;

    cout << output;
    return 0;
}
