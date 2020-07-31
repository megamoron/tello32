#pragma once

bool XInputInit();
bool XInputUninit();
void XInputPoll(int ms); // only returns when input ended

bool DirectInputInit();
bool DirectInputUninit();
void DirectInputPoll(int ms); // only returns when input ended