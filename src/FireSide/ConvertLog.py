# Python Script to Read Binary Log Files Written by the FireSide PCB

#### Library Imports
# Graceful Script Exit
from sys import exit

# CSV Writer Module using Dictionary Container
from csv import DictWriter

# User Interaction for File Location and Status Updates
from tkinter import filedialog, messagebox, Tk

# C/C++ Structure Unpacking Utility
from struct import unpack


# The C/C++ Data Storage Sequence
# uint32_t Timestamp
# uint16_t Buffer[ADC_DMA_BLOCKLEN]


# Display Script Startup
print('#########')
print('FireSide Binary Data File Convertor')
print('#########')
print('')


# Create Background Window Context for TKinter
context = Tk()
context.withdraw()


# Set Number of ADC Channels
ADC_PARALLEL_CHANNELS = 5

# Define Maximum Number of ADC Channels
# See Interfaces.hpp
MAX_PARALLEL_CHANNELS = 7
assert ADC_PARALLEL_CHANNELS < MAX_PARALLEL_CHANNELS

# Calculate Buffer Length
# Offset by 4 Bytes for uint32_t Variable Sequence
# See ConvertLog Function in DMADAQ.cpp
ADC_DMA_BLOCKLEN = (ADC_PARALLEL_CHANNELS * 512 - 4)

# Print Configuration and Notify User
print('>> Converter Settings')
print('ADC Parallel Channels: ' + str(ADC_PARALLEL_CHANNELS))
print('ADC DMA Block Size: ' + str(ADC_DMA_BLOCKLEN))
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
    # Read Block from Log File
    # Remove Offset and Calculate Size of Block in Bytes
    buffer = LogFile.read(4 + 2 * ADC_DMA_BLOCKLEN)

    # Check for End of File
    if not buffer:
      break

    # Decode the DMA Buffer and Unpack the Tuple
    # See C/C++ Structure at Start of Script
    buffer = unpack(
      f'<I{ADC_DMA_BLOCKLEN}H', buffer
    )
    TimeStamp = buffer[0]
    data = buffer[1:]

    # Process Each ADC Sample in the DMA buffer in Blocks
    for index in range(0, len(data), ADC_PARALLEL_CHANNELS):
      # Clear Dictionary Buffer
      CurrentRow = {}

      # Calculate the TimeStamp for the Current Sample Block
      CurrentRow.update(
        {'Time (us)' : int((TimeStamp - time) / ADC_DMA_BLOCKLEN * index)}
      )

      # Deinterleave and Append ADC Sample Data to Dictionary
      # NOTE : See Interfaces.hpp
      for channel in range(ADC_PARALLEL_CHANNELS):
        # Decode Data From A0 Onwards in Pinout
        CurrentRow.update(
          {'A' + str(channel) : data[index + channel]}
        )

      # Append Converted ADC Sample Data to Table
      CSVDataTable.append(CurrentRow)

    # Update the TimeStamp for the Next Block
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
