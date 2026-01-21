#### Python GroundSide Front End for the FireSide PCB


#### Library Imports
# Output and Execution Logging
from datetime import datetime
import logging

# PySerial Interface and Delays for RYLR Communication
from serial import Serial
from time import sleep

# Serial COM Port Selection
from serial.tools.list_ports import comports

# Launch Confirmation Sequence Generation
from secrets import choice
from string import digits

# Graceful Script Termination
from sys import exit

#### Setup Logging for Execution Run
# Assemble Logfile Name
# Reference for Format Codes:
# https://docs.python.org/3/library/datetime.html#strftime-and-strptime-behavior
LogfileName = datetime.now().astimezone().strftime(
  r'GroundSide-%Y%m%d-%H%M%S%Z.log'
)

# Create Logger Instance
# See https://docs.python.org/3/howto/logging.html#logging-basic-tutorial
try:
  # Configure Logging Format & Filename
  logging.basicConfig(
    level=logging.INFO,
    filename=LogfileName,
    filemode='x',
    format=r'%(asctime)s : %(message)s',
    datefmt=r'%Y/%m/%d-%H%M%S%Z'
  )

  # Test Logger Instance
  Logger = logging.getLogger()
  Logger.info('LOG START')

  # Define Wrapper for Console Print Function
  def LoggedPrint(*args, **kwargs) -> None:
    DataString = ' '.join(str(a) for a in args)
    print(DataString, **kwargs)
    Logger.info(DataString.strip())
except Exception as e:
  # Handle Unexpected Error
  print(f'\nUnable to Start Logger: Unexpected {e}')
  # Do Not Proceed
  input('!!!! Press Any Key to Exit')
  exit()


#### Display Startup to User
LoggedPrint('\n#########')
LoggedPrint(' GroundSide: An Interface to the FireSide PCB')
LoggedPrint('#########')


#### Setup COM Port
# Ask User to Select COM Port
LoggedPrint('\nLoaded COM Ports:')
for index, port in zip(range(len(comports())), comports()):
  LoggedPrint(index, ' : ', port)

PortIndex = input('\nEnter COM Device Index: ')

# Check User Input String
try:
  # Verify the Selected Port Exists
  COMPort = comports()[int(PortIndex)]
  LoggedPrint('Selected Device: ', COMPort)
except IndexError, ValueError:
  # Notify User of Invalid Input
  LoggedPrint('\n!!!! Invalid COM Port Index Entered: ' + PortIndex)
  input('!!!! Press Any Key to Exit')
  exit()

# Notify User of Baud Rate Defaults
# See REYAX RYLR998 Datasheet for UART Configuration Defaults
LoggedPrint('\nDefault Baud Rate for RYLR998: 115200')
# See REYAX RYLR993 Datasheet for UART Configuration Defaults
LoggedPrint('Default Baud Rate for RYLR993: 9600')

# Ask User for Port Baud Rate and Validate Input
RYLR_UART_BAUD = input('Enter Port Baud Rate: ')
try:
  LoggedPrint('Selected Baud Rate: ', int(RYLR_UART_BAUD))
except ValueError:
  # Notify User of Invalid Input
  LoggedPrint('\n!!!! Invalid Port Baud Rate Entered: ' + RYLR_UART_BAUD)
  input('!!!! Press Any Key to Exit')
  exit()

# Notify User of Serial Startup
LoggedPrint('\nStarting Serial on ' + COMPort[0])

# Create and Configure Serial Object
RYLR = Serial(
  port=COMPort[0],
  baudrate=int(RYLR_UART_BAUD),
  timeout=0.5
)


#### Define Interface Layer Functions to RYLR Module
# Poll RYLR module & Parse Incoming Data from FireSide PCB
def ParseRYLR() -> str:
  # Setup Receive Buffer
  parsed = ''

  # Load Incoming Binary Data
  # Ignore any RYLR Responses
  while not parsed.startswith('+RCV'):
    # Wait for FireSide to Respond
    sleep(0.25)

    # Ignore Bad Bytes during Conversion
    parsed = RYLR.read_until(b'\n').decode('utf-8', errors='ignore')

  # Validate the Data Format
  # See +RCV in REYAX AT RYLRX93 Commanding Datasheet
  # https://reyax.com//products/RYLR993
  if parsed.count(',') == 4:
    # Extract the 2nd, 3rd & 4th Comma Separated Fields
    _, length, data, signal, _ = parsed.split(',', maxsplit=4)

    # Check for Missing Characters
    try:
      if len(data) == int(length):
        # Received Data is Valid
        # Also Return Received Signal Strength at GroundSide
        return f'{data} (RSSI: {signal} dBm)'
    except ValueError:
      # Response Length Data Corrupted
      # No Recovery Needed Here
      # Final Return Handles Invalid Packets
      pass

  # Invalid +RCV Command
  return f'\n!!!! Malformed +RCV Response: {parsed}\n'


# Sends State Commands to FireSide PCB via RYLR module
def SendRYLR(State : str):
  # Setup Response Buffer
  response = ''

  # Check for Invalid Commands or Switches
  OverrideResponse = False

  # Validate State Command
  if State not in ['SAFE', 'ARM', 'LAUNCH', 'CONVERT']:
    LoggedPrint(f'\n!!!! Invalid Command To FireSide: {State}')
    OverrideResponse = True

  # Confirm Entry into ARM State
  if State == 'ARM':
    # Generate and Output OPT for User
    OTP : str = ''.join(choice(digits) for i in range(6))

    LoggedPrint('\nOTP for ARM State Transition: ' + OTP)

    # Check User Entry Against OTP
    if input('Please Re-enter OTP to Confirm: ') != OTP:
      LoggedPrint('\n!!!! ARM OTP Invalid. Safing FireSide!')
      OverrideResponse = True

  # Confirm Entry into LAUNCH State
  if State == 'LAUNCH':
    # Generate and Output OPT for User
    OTP : str = ''.join(choice(digits) for i in range(6))

    LoggedPrint('\nOTP for LAUNCH State Transition: ' + OTP)

    # Check User Entry Against OTP
    if input('Please Re-enter OTP to Confirm: ') != OTP:
      LoggedPrint('\n!!!! LAUNCH OTP Invalid. Safing FireSide!')
      OverrideResponse = True

  # Clear RYLR Serial Write Buffer
  RYLR.flush()

  # Wait for Response from RYLR
  sleep(0.25)

  # Clear Existing Data in COM Serial Read Buffer
  while RYLR.in_waiting:
    RYLR.read()

  # Default to SAFE State if Above Checks Fail
  if OverrideResponse:
    LoggedPrint('\nSending SAFE Command')

    # Issue Send AT Command
    # See +SEND in REYAX AT RYLRX93 Commanding Datasheet
    # https://reyax.com//products/RYLR993
    RYLR.write('AT+SEND=0,'.encode())

    # Issue Payload Length
    # 4 Characters for SAFE Command
    # Complete Binary Command with Mandatory CRLF Line End
    RYLR.write('4,SAFE\r\n'.encode())
  else:
    LoggedPrint(f'\nSending {State} Command')

    # Issue Send AT Command
    # See +SEND in REYAX AT RYLRX93 Commanding Datasheet
    # https://reyax.com//products/RYLR993
    RYLR.write('AT+SEND=0,'.encode())

    # Issue Payload Length
    RYLR.write(str(len(State)).encode())

    # Complete Binary Command with Comma and Mandatory CRLF Line End
    RYLR.write((',' + State + '\r\n').encode())

  # Wait for RYLR Response to SEND Command
  # Ignore Whitespace and Newlines
  while len(response.strip()) == 0:
    # Allow RYLR to Respond
    sleep(0.25)

    # Ignore any Bad Bytes during Conversion
    response = RYLR.read_until(b'\n').decode('utf-8', errors='ignore')

  # Check Response & Notify User if Transmission Fails
  # See +SEND in REYAX AT RYLRX93 Commanding Datasheet
  # https://reyax.com//products/RYLR993
  if 'OK' not in response:
    LoggedPrint(f'\n!!!! RYLR Commanding Failed. Response: {response}\n')

  return


#### Establish Communication via RYLR module
# See AT in REYAX AT RYLRX93 Commanding Datasheet
# https://reyax.com//products/RYLR993
LoggedPrint('\nEstablishing RYLR Link')
RYLR.write('AT\r\n'.encode())

# Wait Until RYLR Responds with "OK"
while 'OK' not in RYLR.read_until(b'\n').decode('utf-8', errors='ignore'):
  sleep(0.25)

#### Start RYLR Communication Loop
# Allow Graceful Termination with Ctrl+C Interrupt
LoggedPrint('Starting RYLR Communication Loop with FireSide')
LoggedPrint('Ctrl+C to Exit Communication Loop\n')

# Prompt User for FireSide PCB Initial State
# Send the Initial State
SendRYLR(input('Choose Initial State (SAFE || CONVERT): '))

try:
  # Initialise Single Line Buffers for RYLR Data
  RXBuffer = ''
  TXBuffer = ''

  while True:
    # Poll for Incoming Data from FireSide PCB
    RXBuffer = ParseRYLR()
    LoggedPrint(RXBuffer)

    # Check Last Line for Request for Commands from FireSide PCB
    if 'FS> REQUEST COMMAND' in RXBuffer:
      # Block for Input and Fill Line Buffer
      TXBuffer = input('Enter Command [Case Sensitive]: ')

      # Send Command while Ignoring New Lines
      if TXBuffer and TXBuffer.strip():
        SendRYLR(TXBuffer)

# Graceful Exit on Ctrl+C Interrupt
except KeyboardInterrupt:
    LoggedPrint('\nStopping GroundSide Control\n')
    RYLR.close()
    exit()
