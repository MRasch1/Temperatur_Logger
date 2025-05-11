// Get references to the DOM elements for displaying temperature and the chart
const temperatureElement = document.getElementById("temperature");
const chartCanvas = document.getElementById("chart");

// Initialize WebSocket connection to the ESP32 WebSocket server
const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

// Handle incoming WebSocket messages (temperature updates from the ESP32)
ws.onmessage = (event) => {
  const temperature = parseFloat(event.data); // Parse the temperature value from the WebSocket message
  temperatureElement.textContent = temperature.toFixed(2); // Update the temperature display in the UI
  updateChart(temperature); // Update the chart with the new temperature value
};

// Handle WebSocket connection errors
ws.onerror = (error) => {
  console.error("WebSocket Error:", error); // Log the error to the console
};

// Handle WebSocket connection closure
ws.onclose = () => {
  console.warn("WebSocket connection closed."); // Log a warning when the connection is closed
};

// Chart.js setup for displaying temperature data over time
let chartData = {
  labels: [], // Array to store time labels for the x-axis
  datasets: [{
    label: "Temperature (°C)", // Label for the dataset
    data: [], // Array to store temperature data points
    borderColor: "rgba(75, 192, 192, 1)", // Line color
    backgroundColor: "rgba(75, 192, 192, 0.2)", // Fill color under the line
    borderWidth: 1, // Line thickness
    fill: true, // Enable fill under the line
  }]
};

// Create a new Chart.js line chart
const chart = new Chart(chartCanvas, {
  type: "line", // Chart type: line chart
  data: chartData, // Data for the chart
  options: {
    responsive: true, // Make the chart responsive to screen size
    scales: {
      x: {
        title: {
          display: true, // Display the x-axis title
          text: "Time" // Text for the x-axis title
        }
      },
      y: {
        title: {
          display: true, // Display the y-axis title
          text: "Temperature (°C)" // Text for the y-axis title
        },
        beginAtZero: false // Do not force the y-axis to start at zero
      }
    }
  }
});

// Function to update the chart with new temperature data
function updateChart(temperature) {
  const now = new Date(); // Get the current date and time

  // Format the date as DD/MM/YYYY
  const day = String(now.getDate()).padStart(2, '0');
  const month = String(now.getMonth() + 1).padStart(2, '0'); // Month is 0-based
  const year = now.getFullYear();

  const formattedDate = `${day}/${month}/${year}`; // Combine day, month, and year
  const formattedTime = now.toLocaleTimeString(); // Format the time (e.g., HH:MM:SS)

  const dateTimeLabel = `${formattedDate} ${formattedTime}`; // Combine date and time into a single label

  chartData.labels.push(dateTimeLabel); // Add the formatted label to the x-axis labels
  chartData.datasets[0].data.push(temperature); // Add the temperature value to the dataset

  // Limit the chart to display the last 30 data points
  if (chartData.labels.length > 30) {
    chartData.labels.shift(); // Remove the oldest label
    chartData.datasets[0].data.shift(); // Remove the oldest data point
  }

  chart.update(); // Refresh the chart to display the new data
}

// Function to activate service mode on the ESP32
function activateServiceMode() {
  fetch('/activate_service_mode') // Send a request to the ESP32 to activate service mode
    .then(response => response.text()) // Parse the response as text
    .then(data => alert(data)) // Show the server's response in an alert box
    .catch(error => console.error('Error:', error)); // Log any errors to the console
}

// Check if the ESP32 is currently in service mode
fetch('/is_service_mode') // Send a request to check the service mode status
  .then(response => response.json()) // Parse the response as JSON
  .then(data => {
    if (data.serviceMode) {
      // If service mode is active, display the service mode buttons
      document.getElementById('service-mode').style.display = 'block';
    } else {
      // If service mode is not active, hide the service mode buttons
      document.getElementById('service-mode').style.display = 'none';
    }
  })
  .catch(error => console.error('Error:', error)); // Log any errors to the console