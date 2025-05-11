# Python Script to Read Binary Log Files Written by the FireSide PCB

#### Library Imports
# Graceful Script Exit
from sys import exit

# CSV Writer Module using Dictionary Container
from csv import DictWriter

# User Interaction for File Location and Status Updates
from tkinter import filedialog, messagebox

# C/C++ Structure Unpacking Utility
from struct import unpack


# The C/C++ DMA Buffer Data Storage Structure
# struct DMABuffer
# {
#   Flag Container to Track Buffer Status
#   uint32_t status;
#   Buffer Write Time Referenced to Controller Power Up
#   uint32_t TimeStamp;
#   Buffer for DMA Transfer of ADC Results
#   uint16_t DMA_ADCBuf[ADC_DMA_BUFLEN];
# } BufferA, BufferB;


# Display Script Startup
print('#########')
print('FireSide Binary Data File Convertor')
print('#########')
print('')


# Set Number of ADC Channels
ADC_PARALLEL_CHANNELS = 8

# Calculate Buffer Length
# Offset by 4 bytes for uint32_t Variables in Structure
# See ConvertLog Function in DMADAQ.cpp
ADC_DMA_BUFLEN = (ADC_PARALLEL_CHANNELS * 512 - 4)

# Print Configuration and Notify User
print('>> Converter Settings')
print('ADC Parallel Channels: ' + str(ADC_PARALLEL_CHANNELS))
print('ADC DMA Buffer Size: ' + str(ADC_DMA_BUFLEN))
print('')


# Ask User for Path to Input Binary File
print('>> Input File Path [Select in Popup]')
LogPath = filedialog.askopenfilename(
  title='Select Binary Log File',
  filetypes=[('FireSide Binary Log Files', '*.dat')]
)
print('File Location: ' + str(LogPath))

if not LogPath:
  print('No File Found at Specified Path')
  messagebox.showerror(
    title='Binary Input File Not Found',
    message=str(LogPath) + ' is Not Valid'
  )
  exit()


# Allocate the CSV Data Containers
CurrentRow = {}
CSVDataTable = []


# Begin Conversion Procedure
# Iterate Through All Logged DMA Buffers
print('>> Reading and Converting Binary File')
with open(LogPath, 'rb') as LogFile:
  # Allocate Buffers for Binary Data
  time = 0

  # Read Starting Time from 1st 4 Bytes
  # See TriggerLogging() in DMADAQ.cpp
  time = unpack('<I', LogFile.read(4))[0]

  while True:
    # Read DMA Buffer from Log File
    # Remove Offset and Calculate Size of Entire Structure
    buffer = LogFile.read(2 * (ADC_DMA_BUFLEN + 4))

    # Check for End of File
    if not buffer:
      break

    # Decode the DMA Buffer and Unpack the Tuple
    # See C/C++ Structure at Start of Script
    buffer : tuple = unpack(
      f'<II{ADC_DMA_BUFLEN}H', buffer
    )
    status = buffer[0]
    TimeStamp = buffer[1]
    data = buffer[2:]

    # Process Each ADC Sample in the DMA buffer in Blocks
    for index in range(0, len(data), ADC_PARALLEL_CHANNELS):
      # Clear Dictionary Buffer
      CurrentRow = {}

      # Calculate the TimeStamp for the Current Sample Block
      CurrentRow.update(
        {'Time (us)' : int((TimeStamp - time) / ADC_DMA_BUFLEN * index)}
      )

      # Deinterleave and Append ADC Sample Data to Dictionary
      # NOTE : Channels with Junk Data are Skipped
      for channel in range(ADC_PARALLEL_CHANNELS):
        # Decode Data from A3 to A6 in Pinout
        if channel < 4:
          CurrentRow.update(
            {'A' + str(channel + 3) : data[index + channel]}
          )
        # Decode Data From A1 and A2 in Pinout
        elif channel > 5:
          CurrentRow.update(
            {'A' + str(channel - 5) : data[index + channel]}
          )

      # Append Converted ADC Sample Data to Table
      CSVDataTable.append(CurrentRow)

    # Update the TimeStamp for the Next DMA Buffer
    time = TimeStamp



# Ask User for Path to Output Binary File
print('>> Output File Path [Select in Popup]')
CSVPath = filedialog.asksaveasfilename(
  title='Input CSV File Name',
  filetypes=[('FireSide CSV Log Files', '*.csv')]
)
print('CSV File Location: ' + str(CSVPath))

if not CSVPath:
  print('Specified Path is Not Accessible')
  messagebox.showerror(
    title='CSV Output File Name Error',
    message=str(CSVPath) + ' is Not Valid'
  )
  exit()


# Write Converted Data to Selected CSV File
with open(CSVPath, 'w') as CSVFile:
  TableWriter = DictWriter(
    CSVFile,
    fieldnames=CSVDataTable[0].keys(),
    lineterminator='\r\n'
  )

  # Write CSV Header
  TableWriter.writeheader()

  # Write Converted Data
  TableWriter.writerows(CSVDataTable)
