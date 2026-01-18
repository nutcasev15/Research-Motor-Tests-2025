#### Python GroundSide Front End for the FireSide PCB


#### Library Imports
# Serial and Delays for RYLR Communication
from serial import Serial
from time import sleep

# Serial COM Port Selection
from serial.tools.list_ports import comports

# Launch Confirmation Sequence Generation
from secrets import choice
from string import digits

# Graceful Script Termination
from sys import exit


#### Display Startup to User
print('\n---')
print(' GroundSide: An Interface to the FireSide PCB')
print('---')


#### Setup COM Port
# Ask User to Select COM Port
print('\nLoaded COM Ports:')
for index, port in zip(range(len(comports())), comports()):
  print(index, ' : ', port)

PortIndex = input('\nEnter COM Device Index: ')

# Check User Input String
try:
  # Verify the Selected Port Exists
  COMPort = comports()[int(PortIndex)]
  print('Selected Device: ', COMPort)
except IndexError:
  # Notify User of Invalid Input
  print('\n!!!! Invalid COM Port Index Entered: ' + PortIndex)
  input('!!!! Press Any Key to Exit')
  exit()

# Notify User of Baud Rate Defaults
# See REYAX RYLR998 Datasheet for UART Configuration Defaults
print('\n Default Baud Rate for RYLR998: 115200')
# See REYAX RYLR993 Datasheet for UART Configuration Defaults
print('Default Baud Rate for RYLR993: 9600')

# Ask User for Port Baud Rate and Validate Input
RYLR_UART_BAUD = input('Enter Port Baud Rate: ')
try:
  RYLR_UART_BAUD = int(RYLR_UART_BAUD)
except ValueError:
  # Notify User of Invalid Input
  print('\n!!!! Invalid Port Baud Rate Entered: ' + RYLR_UART_BAUD)
  input('!!!! Press Any Key to Exit')
  exit()

# Notify User of Serial Startup
print('\nStarting Serial on ' + COMPort[0])

# Create and Configure Serial Object
RYLR = Serial(
  port=COMPort[0],
  baudrate=RYLR_UART_BAUD,
  timeout=0.5
)


#### Define Interface Layer Functions to RYLR Module
# Poll RYLR module & Parse Incoming Data from FireSide PCB
def ParseRYLR() -> str:
  # Setup Receive Buffer
  parsed = ''

  # Load Incoming Binary Data
  # Check if Data is from FireSide PCB & Ignore any RYLR Responses
  while not parsed.startswith('+RCV='):
    # Wait for FireSide to Respond
    sleep(0.5)

    # Ignore Bad Bytes during Conversion
    parsed = RYLR.read_until(b'\n').decode('utf-8', errors='ignore')

    # Validate the Data Format
    # See +RCV in REYAX AT RYLRX93 Commanding Datasheet
    # https://reyax.com//products/RYLR993
    if parsed.startswith('+RCV=') and parsed.count(',') == 4:
      # Extract & Return the Data in 3rd Comma Separated Field
      # Also Return RSSI in 4th Comma Separated Field
      return f'{parsed.split(',', maxsplit=4)[2]} \
        (RSSI: {parsed.split(',', maxsplit=4)[3]} dBm)'

  # Invalid RCV Command
  return f'\n!!!! Malformed +RCV Response: {parsed}\n'


# Sends State Commands to FireSide PCB via RYLR module
def SendRYLR(State : str):
  # Setup Response Buffer
  response = ''

  # Check for Invalid Commands or Switches
  OverrideResponse = False

  # Validate State Command
  if State not in ['SAFE', 'ARM', 'LAUNCH', 'CONVERT']:
    print('\n!!!! Invalid Command To FireSide')
    OverrideResponse = True

  # Confirm Entry into ARM State
  if State == 'ARM':
    # Generate and Output OPT for User
    OTP : str = ''.join(choice(digits) for i in range(6))

    print('\nOTP for ARM State Transition: ' + OTP)

    # Check User Entry Against OTP
    if input('Please Re-enter OTP to Confirm: ') != OTP:
      print('\n!!!! ARM OTP Invalid. Safing FireSide!')
      OverrideResponse = True

  # Confirm Entry into LAUNCH State
  if State == 'LAUNCH':
    # Generate and Output OPT for User
    OTP : str = ''.join(choice(digits) for i in range(6))

    print('\nOTP for LAUNCH State Transition: ' + OTP)

    # Check User Entry Against OTP
    if input('Please Re-enter OTP to Confirm: ') != OTP:
      print('\n!!!! LAUNCH OTP Invalid. Safing FireSide!')
      OverrideResponse = True

  # Clear RYLR Serial Write Buffer
  RYLR.flush()

  # Wait for Response from RYLR
  sleep(0.5)

  # Clear Existing Data in COM Serial Read Buffer
  while RYLR.in_waiting:
    RYLR.read()

  # Default to SAFE State if Above Checks Fail
  if OverrideResponse:
    print('\nSending SAFE Command')

    # Issue Send AT Command
    # See +SEND in REYAX AT RYLRX93 Commanding Datasheet
    # https://reyax.com//products/RYLR993
    RYLR.write('AT+SEND=0,'.encode())

    # Issue Payload Length
    # 4 Characters for SAFE Command
    # Complete Binary Command with Mandatory CRLF Line End
    RYLR.write('4,SAFE\r\n'.encode())
  else:
    # Issue Send AT Command
    # See +SEND in REYAX AT RYLRX93 Commanding Datasheet
    # https://reyax.com//products/RYLR993
    RYLR.write('AT+SEND=0,'.encode())

    # Issue Payload Length
    RYLR.write(str(len(State)).encode())

    # Complete Binary Command with Comma and Mandatory CRLF Line End
    RYLR.write((',' + State + '\r\n').encode())

  # Wait for RYLR Response to SEND Command
  while len(response) == 0:
    # Allow RYLR to Respond
    sleep(0.5)

    # Ignore any Bad Bytes during Conversion
    response = RYLR.read_until(b'\n').decode('utf-8', errors='ignore')

    # Remove Whitespace and Newlines from RYLR Response
    response.strip()

  # Check Response & Notify User if Transmission Fails
  # See +SEND in REYAX AT RYLRX93 Commanding Datasheet
  # https://reyax.com//products/RYLR993
  if 'OK' not in response:
    print(f'\n!!!! RYLR Commanding Failed. Response: {response}\n')

  return


#### Establish Communication via RYLR module
# See AT in REYAX AT RYLRX93 Commanding Datasheet
# https://reyax.com//products/RYLR993
print('\nEstablishing RYLR Link')
RYLR.write('AT'.encode())

# Wait Until RYLR Begins Response with "OK"
while RYLR.read().decode('utf-8', errors='ignore') != 'O':
  sleep(0.5)


#### Start RYLR Communication Loop
# Allow Graceful Termination with Ctrl+C Interrupt
print('Starting RYLR Communication Loop with FireSide')
print('Ctrl+C to Exit Communication Loop\n')

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
    print(RXBuffer)

    # Check Last Line for Request for Commands from FireSide PCB
    if RXBuffer == 'FS> REQUEST COMMAND':
      # Block for Input and Fill Line Buffer
      TXBuffer = input('Enter Command: ')

      # Send Command while Ignoring New Lines
      if TXBuffer and TXBuffer.strip():
        SendRYLR(TXBuffer)

# Graceful Exit on Ctrl+C Interrupt
except KeyboardInterrupt:
    print('\nStopping GroundSide Control\n')
    RYLR.close()
    exit()
