#### Python GroundSide Front End for the FireSide PCB


#### Library Imports
# Serial and Delays for RYLR Communication
from serial import Serial
from time import sleep

# Serial COM Port Selection
from serial.tools.list_ports import comports

# Launch Confirmation Sequence Generation
from secrets import choice
from string import ascii_letters, digits, punctuation

# Graceful Script Termination
from sys import exit


#### Display Startup to User
print('\n---')
print(' GroundSide: An Interface to the FireSide PCB')
print('---')


#### Setup COM Port
# Ask User to Select COM Port
print('\nLoaded COM Ports:')
for port in comports():
  print(port)

PortID = input('\nEnter COM Port Number:')

# Check User Input String
try:
  # Verify the Selected Port Exists
  if not (('COM' + PortID) in [port.name for port in comports()]):
    raise IndexError

except IndexError:
  # Notify User of Invalid Input
  print('\n!!!! Invalid COM Port ID Entered: ' + PortID)
  input('!!!! Press Any Key to Exit')
  exit()

# Notify User of Serial Startup
print('\nStarting Serial on COM' + PortID)

# Create and Configure Serial Object for RYLR998
# See REYAX RYLR998 Datasheet for UART Configuration Defaults
RYLR_UART_BAUD = 115200
RYLR = Serial(
  port='COM' + PortID,
  baudrate=RYLR_UART_BAUD,
  timeout=0.1
)


#### Define Interface Layer Functions to RYLR998 Module
# Parses Incoming Data from FireSide PCB via RYLR module
def ParseRYLR() -> str:
  if not RYLR.in_waiting:
    # Return Blank Buffer
    return str('\n')

  # Load Incoming Binary Data
  parsed = RYLR.read_until(b'\n').decode()

  # See +RCV in REYAX AT RYLRX98 Commanding Datasheet
  # Extract Data in 3rd Comma Separated Field
  parsed = parsed.split(',', maxsplit=4)[2]

  # Strip and Return Parsed Data
  return parsed.strip()

# Sends State Commands to FireSide PCB via RYLR module
def SendRYLR(State : str):
  # Check for Invalid Commands or Switches
  OverrideResponse = False

  # Validate State Command
  if State not in ['SAFE', 'ARM', 'LAUNCH', 'CONVERT']:
    print('\n!!!! Invalid Command To FireSide')
    OverrideResponse = True

  # Confirm Entry into ARM State
  if State == 'ARM':
    # Generate and Output OPT for User
    OTP : str = ''.join(choice(
      ascii_letters + digits + punctuation
    ) for i in range(8))

    print('\nOTP for ARM State Transition: ' + OTP)

    # Check User Entry Against OTP
    if input('Please Re-enter OTP to Confirm:') != OTP:
      print('\n!!!! ARM OTP Invalid. Safing FireSide!')
      OverrideResponse = True

  # Confirm Entry into LAUNCH State
  if State == 'LAUNCH':
    # Generate and Output OPT for User
    OTP : str = ''.join(choice(
      ascii_letters + digits + punctuation
    ) for i in range(8))

    print('\nOTP for LAUNCH State Transition: ' + OTP)

    # Check User Entry Against OTP
    if input('Please Re-enter OTP to Confirm:') != OTP:
      print('\n!!!! LAUNCH OTP Invalid. Safing FireSide!')
      OverrideResponse = True

  # Issue Send AT Command
  # See +SEND in REYAX AT RYLRX98 Commanding Datasheet
  RYLR.write('AT+SEND=0,'.encode())

  # Default to SAFE State if Above Checks Fail
  if OverrideResponse:
    print('\nSending SAFE Command')

    # Issue Payload Length
    # 4 Characters for SAFE Command
    # Complete Binary Command with Line End
    RYLR.write('5,SAFE\n'.encode())
  else:
    # Issue Payload Length
    RYLR.write(str(len(State + '\n')).encode())

    # Complete Binary Command with Comma and Line End
    RYLR.write((',' + State + '\n').encode())

  return


#### Establish Communication via RYLR module
print('\nEstablishing FireSide Link')

# Prompt User for FireSide PCB Initial State
# Send the Initial State
SendRYLR(input('Choose Initial State: SAFE || CONVERT'))

# Wait Until FireSide Begins Response to State Command
while not RYLR.in_waiting:
  sleep(0.5)

print('\nFireSide Link Acquired')


#### Start RYLR Communication Loop
while True:
  # Check for Incoming Data from FireSide PCB
  # Parse and Print Data to the Terminal
  while RYLR.in_waiting:
    # Load and Display Line
    print(ParseRYLR())

  # Check for Incoming Commands from the Terminal
  # Send Input Command
  SendRYLR(input())

  # Wait Until FireSide Begins Response to Sent Command
  while not RYLR.in_waiting:
    sleep(0.5)
