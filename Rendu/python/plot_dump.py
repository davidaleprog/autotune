import numpy as np
from matplotlib import pyplot as plt
from scipy.io.wavfile import write

# --> Adapt the file name (and path) <--
filename_in = 'rtaudio-6.0.1/tests/buffer_dump_in'
filename_out = 'rtaudio-6.0.1/tests/buffer_dump_out'

# Read binary file (double format).
x_in = np.fromfile(filename_in + ".bin", dtype=np.double)
x_out = np.fromfile(filename_out + ".bin", dtype=np.double)

# Write signal as a .wav file (useless if signal is not audio). 
# The sampling rate is hard coded here, --> change it if necessary <--
write(filename_in + '.wav', 44100, x_in.astype(np.float32))
write(filename_out + '.wav', 44100, x_out.astype(np.float32))

# Plot the signal. --> Adapt the text on the Figure <--
plt.subplot(211)
plt.plot(x_in)
plt.title('Input audio signal')
plt.xlabel('time')
plt.ylabel('Amplitude')
plt.grid()
plt.subplot(212)
plt.plot(x_out)
plt.title('Output buffer')
plt.xlabel('time')
plt.ylabel('Amplitude')

plt.grid()
plt.show()