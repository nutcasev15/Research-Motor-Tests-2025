import struct
import csv
from tkinter import Tk, filedialog

# Define the number of ADC channels and buffer length
ADC_PARALLEL_CHANNELS = 8
ADC_DMA_BUFLEN = (ADC_PARALLEL_CHANNELS * 512 - 4)  # 4 bytes for uint32_t

def convert_log():
    # Hide the root window
    root = Tk()
    root.withdraw()

    # Open file dialog to select the binary file
    file_path = filedialog.askopenfilename(title="Select the binary log file", filetypes=[("Binary files", "*.dat")])
    if not file_path:
        print("No file selected.")
        return

    # Generate the CSV file name
    csv_file_path = file_path.replace(".dat", ".csv")

    try:
        with open(file_path, "rb") as log_file, open(csv_file_path, "w", newline='') as csv_file:
            fieldnames = ["time (us)"] + [f"A{3 + channel}" for channel in range(ADC_PARALLEL_CHANNELS)]
            csv_writer = csv.DictWriter(csv_file, fieldnames=fieldnames)

            # Write the CSV header
            csv_writer.writeheader()

            while True:
                # Read the status and timestamp
                status_time_data = log_file.read(8)  # 4 bytes for status and 4 bytes for timestamp
                if not status_time_data:
                    break

                status, timestamp = struct.unpack('II', status_time_data)

                # Read the DMA buffer
                buffer_data = log_file.read(ADC_DMA_BUFLEN * 2)  # 2 bytes per uint16_t
                if not buffer_data:
                    break

                # Unpack the DMA buffer
                adc_values = struct.unpack(f'{ADC_DMA_BUFLEN}H', buffer_data)

                # Calculate the relative time and write the row to the CSV
                row = {"time (us)": timestamp}
                row.update({f"A{3 + channel}": adc_values[channel] for channel in range(ADC_PARALLEL_CHANNELS)})
                csv_writer.writerow(row)

        print(f"Conversion complete. CSV file saved as {csv_file_path}")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    convert_log()
