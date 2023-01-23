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
#include <climits>
#include <fstream>
#include <iostream>
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
    if (argc != 2) {
        cout <<
            "Usage: \n"
            << argv[0] << " file_name\n"
            " file_name: Timecode XML file from MediaInfo\n"
            ;
        return 1;
    }
    ifstream input_file(argv[1]);
    input_file.seekg(0, std::ios::end);
    auto input_size = input_file.tellg();
    if (!input_size || input_size > INT_MAX) {
        return 1;
    }
    input_file.seekg(0);
    auto input = new char[input_size];
    input_file.read(input, input_size);

    tfsxml_string xml_handle, n, v;
    if (tfsxml_init(&xml_handle, input, (int)input_size)) {
        return 1;
    }
    uint64_t time_stamp_inc = 0;
    uint64_t time_stamp_den = 0;
    uint64_t time_stamp_num = 0;
    vector<stream_struct> streams;
    size_t active_stream_count=0;
    while (!tfsxml_next(&xml_handle, &n)) {
        if (!tfsxml_strcmp_charp(n, "timecode_streams")) {
            tfsxml_enter(&xml_handle);
            while (!tfsxml_next(&xml_handle, &n)) {
                if (!tfsxml_strcmp_charp(n, "timecode_stream")) {
                    stream_struct stream;
                    while (!tfsxml_attr(&xml_handle, &n, &v)) {
                        if (!tfsxml_strcmp_charp(n, "frame_count")) {
                            stream.frame_count = atoll(tfsxml_decode(v).c_str());
                        }
                        if (!tfsxml_strcmp_charp(n, "frame_rate")) {
                            string new_frame_rate = tfsxml_decode(v);
                            size_t new_frame_rate_div;
                            auto new_frame_rate_num = stoull(new_frame_rate, &new_frame_rate_div);
                            unsigned long long new_frame_rate_den;
                            if (new_frame_rate_div < new_frame_rate.size() && new_frame_rate[new_frame_rate_div] == '/') {
                                new_frame_rate_div++;
                                new_frame_rate_den = stoull(new_frame_rate.substr(new_frame_rate_div));
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
                                return 1;
                            }
                            stream.timecode.SetFramesMax((new_frame_rate_num + new_frame_rate_den - 1) / new_frame_rate_den);
                        }
                        if (!tfsxml_strcmp_charp(n, "id")) {
                            stream.id = tfsxml_decode(v).c_str();
                        }
                        if (!tfsxml_strcmp_charp(n, "start_tc")) {
                            stream.timecode.FromString(tfsxml_decode(v));
                        }
                    }
                    if (!stream.timecode.GetIsValid()) {
                        stream.xml_handle = xml_handle;
                        if (tfsxml_enter(&stream.xml_handle)) {
                            return 1;
                        }
                        if (tfsxml_next(&stream.xml_handle, &stream.n)) {
                            return 1;
                        }
                    }
                    if (stream.id.size() < 40) {
                        stream.id.insert(stream.id.begin(), 40 - stream.id.size(), ' ');
                    }
                    stream.id.insert(stream.id.begin(), 1, '\n');
                    stream.id += ": ";
                    streams.push_back(stream);
                }
            }
        }
    }
    if (!time_stamp_inc || !time_stamp_den) {
        return 1;
    }

    string output("WEBVTT\n");
    active_stream_count = streams.size();
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

    cout << output;
    return 0;
}