/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <mutex>
void StartCorsairStartupRecovery();
void StopCorsairStartupRecovery();
void WatchMissingCorsairReceiver(const char* path);
void ForgetMissingCorsairReceiver(const char* path);
std::mutex& CorsairReceiverDetectionMutex();
