/* Ricochet - https://ricochet.im/
 * Copyright (C) 2014, John Brooks <john.brooks@dereferenced.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the names of the copyright owners nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include "PendingOperation.h"

PendingOperation::PendingOperation()
    : m_finished(false),mFinishedCallback([](){}), mSuccessCallback([](){}),mErrorCallback([](const std::string&){})
{
}

bool PendingOperation::isFinished() const
{
    return m_finished;
}

bool PendingOperation::isSuccess() const
{
    return m_finished && m_errorMessage.empty();
}

bool PendingOperation::isError() const
{
    return m_finished && !m_errorMessage.empty();
}

std::string PendingOperation::errorMessage() const
{
    return m_errorMessage;
}

void PendingOperation::finishWithError(const std::string &message)
{
    if (message.empty())
        m_errorMessage = "Unknown Error";
    m_errorMessage = message;

    if (!m_finished) {
        m_finished = true;

        mErrorCallback(m_errorMessage);
        mFinishedCallback();
    }
}

void PendingOperation::finishWithSuccess()
{
    assert(m_errorMessage.empty());

    if (!m_finished) {
        m_finished = true;

        if (isSuccess())
            mSuccessCallback();

        mFinishedCallback();
    }
}

