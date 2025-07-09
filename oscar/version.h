/* Version.h
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef VERSION_H
#define VERSION_H

#include <QString>

class Version
{
    friend class VersionTests;
public:
    Version(const QString & version_string);
    const QString PrereleaseType() const;
    bool IsReleaseVersion() const { return mPrerelease.isEmpty(); }
    bool IsValid() const { return mIsValid; }
    bool operator==(const Version & b) const { return Compare(*this, b) == 0; }
    bool operator!=(const Version & b) const { return Compare(*this, b) != 0; }
    bool operator<(const Version & b) const { return Compare(*this, b) < 0; }
    bool operator>(const Version & b) const { return Compare(*this, b) > 0; }

    //!brief Returns the full version string, including all metadata, used in reports
    operator const QString &() const;
    //!brief Returns the full version string, including all metadata, used in reports
    const QString & toString() const;
    //!brief Returns the version string to display in the UI, without build metadata if a release version
    const QString displayString() const;
    //!brief Returns the version string without any build metadata
    const QString minimalString() const;

protected:
    const QString mString;
    bool mIsValid;
    
    int mMajor, mMinor, mPatch;
    QString mPrerelease, mBuild;
    
    void ParseSemanticVersion();
    void FixLegacyVersions();
    static int Compare(const Version & a, const Version & b);
};

//!brief Get the current version of the application.
const Version & getVersion();

//!brief Get the date and time of the application was built.
const QString & getBuildDateTime();

QString getPrereleaseSuffix();

#endif // VERSION_H
