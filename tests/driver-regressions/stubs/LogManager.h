#pragma once
extern unsigned int logged_errors;
#define LOG_ERROR(...) (++logged_errors)

#define LOG_INFO(...) ((void)0)

#define LOG_WARNING(...) ((void)0)
