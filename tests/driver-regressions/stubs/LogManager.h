#pragma once
extern unsigned int logged_errors;
#define LOG_ERROR(...) (++logged_errors)
