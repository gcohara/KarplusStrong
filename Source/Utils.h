//
//  Utils.h
//  KarplusStrong
//
//  Created by George O'Hara on 26/01/2023.
//

#ifndef Utils_h
#define Utils_h

class RingBuffer {
    // will fail in the niche use case of 96kHz and notes below midi G-1 (24.5Hz)
    std::array<float, 4096> buffer;
    std::array<float, 4096> processingBuffer;
    juce::AbstractFifo abstractFifo { 4096 };
    juce::Random random;
public:
    void reset() {
        abstractFifo.reset();
    }
    void setSize(int newSize) {
        abstractFifo.setTotalSize(newSize);
    }
    float nextSample() {
        auto readHandle = abstractFifo.read(1);
        if (readHandle.blockSize1 == 1) {
            return buffer[readHandle.startIndex1];
        } else {
            return buffer[readHandle.startIndex2];
        }
    }
    
    void writeSample(float sample) {
        auto writeHandle = abstractFifo.write(1);
        if (writeHandle.blockSize1 == 1) {
            buffer[writeHandle.startIndex1] = sample;
        } else {
            buffer[writeHandle.startIndex2] = sample;
        }
    }
    
    void setImpulse(int loopSize) {
        setSize(loopSize);
        auto remaining_space = abstractFifo.getFreeSpace();
        while(remaining_space--) {
            writeSample((random.nextFloat() - 0.5) * 1.75);
        }
        filterImpulse(0.5);
    }
    
    void filterImpulse(float pickPosition) {
        auto size = abstractFifo.getTotalSize();
        std::copy(buffer.cbegin(), buffer.cbegin() + size, processingBuffer.begin());
        // make a big assumption - that after resetting the Fifo and setting a new size etc, read pointer will be at index 0
        for (int i = 0; i < size; i++) {
            processingBuffer[i] = buffer[i] - buffer[i - floor(pickPosition*size)];
        }
        std::copy(processingBuffer.cbegin(), processingBuffer.cbegin() + size, buffer.begin());
    }
};

#endif /* Utils_h */
