/* Overview GUI Headers
 *
 * Copyright (c) 2023-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#ifndef HIGHRESOLUTION_H
#define HIGHRESOLUTION_H

namespace HighResolution {

    // used by main.cpp
    enum HiResolutionMode {HRM_INVALID = 0 , HRM_DISABLED = 1, HRM_ENABLED = 2} ;
    void init();
    void init(HiResolutionMode);
    bool isEnabled();
    void display(bool);
    
    // used by preferences
    bool checkBox(bool set,QCheckBox* button);
}

#endif // HIGHRESOLUTION_H
#endif

