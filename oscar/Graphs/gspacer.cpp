/* graph spacer Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "gspacer.h"

gSpacer::gSpacer(int space)
    : Layer(NoChannel)
{
    m_space = space;
}
