/*
 * Copyright (c) 2025 The OSCAR Team
 *
 * SpeedCheck - Performance monitoring and logging class implementation
 */

#include "SpeedCheck.h"
#include <QDebug>

/**
 * Constructor: Initializes speed check with time limit and optional message
 *
 * @param limit Maximum allowed time in milliseconds
 * @param msg User-friendly message describing what operation is being timed (default: "")
 *
 * Starts the internal high-precision timer and records the start time.
 * Sets default value:
 * - numToLog = 10 (log up to 10 debug messages)
 */
SpeedCheck::SpeedCheck(int limit, const QString &msg)
    : limit(limit)
    , msg(msg)
    , numExceptions(0)
    , numToLog(10)  // Default: log 10 messages
{
    timer.start();
    startTime = timer.elapsed();
}

/**
 * Resets the performance timer to current time
 *
 * Call this before starting a new operation to time.
 * Only resets startTime - keeps limit, msg, and counters unchanged.
 */
void SpeedCheck::restart()
{
    startTime = timer.elapsed();
}

/**
 * Reinitializes the SpeedCheck with new parameters
 *
 * @param limit New time limit in milliseconds
 * @param msg New message to log when limit is exceeded (default: "")
 *
 * Updates the limit and message, then resets the timer.
 * This allows reusing the object with different test parameters.
 */
void SpeedCheck::restart(int limit, const QString &msg)
{
    this->limit = limit;
    this->msg = msg;
    startTime = timer.elapsed();
}

/**
 * Checks if elapsed time exceeds the configured limit
 *
 * Call this after an operation completes to check performance.
 * If the limit is exceeded and logging quota remains:
 *
 * 1. Increments the exception counter (numExceptions++)
 * 2. Logs to qDebug if numExceptions <= numToLog
 *    - If limit is an integral multiple of 1000, reports in seconds
 *    - Otherwise reports in milliseconds
 *    - If limit is 0 or 1000, omits limit from log message
 *    - Includes message and elapsed time
 * 3. When numExceptions reaches numToLog, outputs final "no more exceptions" message
 * 4. Resets startTime to current timer value (auto-restart for next operation)
 *
 * @return Time remaining before limit (negative if limit exceeded)
 */
qint64 SpeedCheck::check()
{
    qint64 currentTime = timer.elapsed();
    qint64 elapsed = currentTime - startTime;
    qint64 remaining = limit - elapsed;

    // Check if operation exceeded the time limit
    if (elapsed > limit) {
        numExceptions++;

        // Log to qDebug if we haven't exceeded the log limit
        if (numExceptions <= numToLog) {
            // Determine if we should report in seconds or milliseconds
            // Check if limit is an integral multiple of 1000
            bool useSeconds = (limit >= 1000 && limit % 1000 == 0);
            bool showLimit = (limit != 0 && limit != 1000);

            if (useSeconds) {
                // Report in seconds
                double elapsedSec = elapsed / 1000.0;
                if (showLimit) {
                    double limitSec = limit / 1000.0;
                    qDebug() << "SpeedCheck:" << msg
                             << "- Elapsed:" << elapsedSec << "sec (over:" << limitSec << "sec)";
                } else {
                    qDebug() << "SpeedCheck:" << msg
                             << "- Elapsed:" << elapsedSec << "sec";
                }
            } else {
                // Report in milliseconds
                if (showLimit) {
                    qDebug() << "SpeedCheck:" << msg
                             << "- Elapsed:" << elapsed << "ms (over:" << limit << "ms)";
                } else {
                    qDebug() << "SpeedCheck:" << msg
                             << "- Elapsed:" << elapsed << "ms";
                }
            }

            // If we just reached the log limit, output a final notice
            if (numExceptions == numToLog) {
                qDebug() << "SpeedCheck: No more exceptions will be reported for:" << msg;
            }
        }
    }

    // Reset startTime for next operation (auto-restart)
    startTime = timer.elapsed();

    // Return time remaining (negative if over limit)
    return remaining;
}

/**
 * Sets message and checks if elapsed time exceeds limit
 *
 * @param msg New message to log when limit is exceeded
 *
 * Updates the message then performs the same check as check().
 *
 * @return Time remaining before limit (negative if limit exceeded)
 */
qint64 SpeedCheck::check(const QString &msg)
{
    this->msg = msg;
    return check();
}
