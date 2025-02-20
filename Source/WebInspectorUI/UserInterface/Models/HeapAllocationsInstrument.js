/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.HeapAllocationsInstrument = class HeapAllocationsInstrument extends WebInspector.Instrument
{
    constructor()
    {
        super();

        console.assert(WebInspector.HeapAllocationsInstrument.supported());

        this._snapshotIntervalIdentifier = undefined;
    }

    // Static

    static supported()
    {
        // COMPATIBILITY (iOS 9): HeapAgent did not exist.
        return window.HeapAgent;
    }

    // Protected

    get timelineRecordType()
    {
        return WebInspector.TimelineRecord.Type.HeapAllocations;
    }

    startInstrumentation()
    {
        // FIXME: Include a "track allocations" option for this instrument.
        // FIXME: Include a periodic snapshot interval option for this instrument.

        HeapAgent.startTracking();

        // Periodic snapshots.
        const snapshotInterval = 10000;
        this._snapshotIntervalIdentifier = setInterval(this._takeHeapSnapshot.bind(this), snapshotInterval);
    }

    stopInstrumentation()
    {
        HeapAgent.stopTracking();

        window.clearInterval(this._snapshotIntervalIdentifier);
        this._snapshotIntervalIdentifier = undefined;
    }

    // Private

    _takeHeapSnapshot()
    {
        HeapAgent.snapshot(function(error, timestamp, snapshotStringData) {
            let workerProxy = WebInspector.HeapSnapshotWorkerProxy.singleton();
            workerProxy.createSnapshot(snapshotStringData, ({objectId, snapshot: serializedSnapshot}) => {
                let snapshot = WebInspector.HeapSnapshotProxy.deserialize(objectId, serializedSnapshot);
                WebInspector.timelineManager.heapSnapshotAdded(timestamp, snapshot);
            });
        });
    }
};
