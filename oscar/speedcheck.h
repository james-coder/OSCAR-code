/*
 * Copyright (c) 2025 The OSCAR Team
 *
 * SpeedCheck - Performance monitoring and logging class
 * Tests whether portions of the application are performing as expected
 * and records to the log when performance limits are exceeded
 */

#ifndef SPEEDCHECK_H
#define SPEEDCHECK_H

#include <QString>
#include <QElapsedTimer>

/**
 * @brief Performance checking class for monitoring operation durations
 *
 * This class tracks elapsed time from instantiation or last restart,
 * and provides debug logging when operations exceed a specified time limit.
 * Designed to help identify performance bottlenecks in the application.
 */
class SpeedCheck
{
public:
    /**
     * @brief Constructs a SpeedCheck object and starts timing
     * @param limit Time limit in milliseconds before warnings are triggered
     * @param msg Message to log when limit is exceeded (default: empty string)
     */
    SpeedCheck(int limit, const QString &msg = "");

    /**
     * @brief Resets the timer to current time
     *
     * Resets startTime to current elapsed time.
     * Allows reusing the same SpeedCheck object for multiple operations.
     */
    void restart();

    /**
     * @brief Reinitializes the SpeedCheck with new limit and message
     * @param limit New time limit in milliseconds
     * @param msg New message to log when limit is exceeded (default: empty string)
     *
     * Resets the timer and updates the limit and message values.
     */
    void restart(int limit, const QString &msg = "");

    /**
     * @brief Checks if elapsed time exceeds limit and logs if needed
     *
     * If limit is exceeded and logging quota not exhausted:
     * - Increments exception counter
     * - Logs to qDebug with elapsed time and limit (if applicable)
     * - If limit is integral multiple of 1000, reports in seconds instead of msec
     * - If limit is 0 or 1000, doesn't show limit in log message
     * - When log limit reached, outputs final "no more exceptions" message
     * - Resets startTime to current timer value after check
     *
     * @return Time remaining before limit (negative if limit exceeded)
     */
    qint64 check();

    /**
     * @brief Sets message and checks if elapsed time exceeds limit
     * @param msg New message to log when limit is exceeded
     *
     * Updates the message then performs the same check as check().
     *
     * @return Time remaining before limit (negative if limit exceeded)
     */
    qint64 check(const QString &msg);

    // Setters
    /**
     * @brief Sets the warning message text
     * @param msg New message to log when limit is exceeded
     */
    void setMsg(const QString &msg) { this->msg = msg; }

    /**
     * @brief Sets the time limit threshold
     * @param limit New time limit in milliseconds
     */
    void setLimit(int limit) { this->limit = limit; }

    /**
     * @brief Sets the maximum number of exceptions to log
     * @param num Maximum number of debug log messages (default is 10)
     */
    void setNumToLog(int num) { numToLog = num; }

    // Getters
    /**
     * @brief Gets the current warning message text
     * @return The message that will be logged
     */
    QString getMsg() const { return msg; }

    /**
     * @brief Gets the current time limit
     * @return Time limit in milliseconds
     */
    int getLimit() const { return limit; }

    /**
     * @brief Gets the maximum number of log messages
     * @return Maximum number of qDebug messages
     */
    int getNumToLog() const { return numToLog; }

    /**
     * @brief Gets the total number of limit exceptions detected
     * @return Total count of times limit was exceeded
     */
    int getNumExceptions() const { return numExceptions; }

private:
    QElapsedTimer timer;        ///< High-precision timer for elapsed time measurement
    qint64 startTime;           ///< Recorded time at start or last restart (milliseconds)
    int limit;                  ///< Time limit in milliseconds before logging triggers
    QString msg;                ///< Message to log when limit is exceeded
    int numExceptions;          ///< Total number of times limit was exceeded
    int numToLog;               ///< Maximum number of qDebug messages to log (default: 10)
};

#endif // SPEEDCHECK_H
