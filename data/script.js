const temperatureElement = document.getElementById("temperature");
const chartCanvas = document.getElementById("chart");

// Initialize WebSocket connection
const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

// Handle incoming WebSocket messages
ws.onmessage = (event) => {
  const temperature = parseFloat(event.data);
  temperatureElement.textContent = temperature.toFixed(2); // Update temperature display
  updateChart(temperature); // Update the chart with the new temperature
};

// Handle WebSocket connection errors
ws.onerror = (error) => {
  console.error("WebSocket Error:", error);
};

// Handle WebSocket connection close
ws.onclose = () => {
  console.warn("WebSocket connection closed.");
};

// Chart.js setup
let chartData = {
  labels: [], // Time labels
  datasets: [{
    label: "Temperature (°C)",
    data: [], // Temperature data
    borderColor: "rgba(75, 192, 192, 1)",
    backgroundColor: "rgba(75, 192, 192, 0.2)",
    borderWidth: 1,
    fill: true,
  }]
};

const chart = new Chart(chartCanvas, {
  type: "line",
  data: chartData,
  options: {
    responsive: true,
    scales: {
      x: {
        title: {
          display: true,
          text: "Time"
        }
      },
      y: {
        title: {
          display: true,
          text: "Temperature (°C)"
        },
        beginAtZero: false
      }
    }
  }
});

// Update the chart with new temperature data
function updateChart(temperature) {
  const now = new Date().toLocaleTimeString(); // Get current time
  chartData.labels.push(now); // Add time label
  chartData.datasets[0].data.push(temperature); // Add temperature data

  // Limit the number of data points displayed
  if (chartData.labels.length > 30) {
    chartData.labels.shift(); // Remove the oldest time label
    chartData.datasets[0].data.shift(); // Remove the oldest temperature data
  }

  chart.update(); // Refresh the chart
}

function activateServiceMode() {
  fetch('/activate_service_mode')
    .then(response => response.text())
    .then(data => alert(data)) // Show the server's response in an alert
    .catch(error => console.error('Error:', error)); // Log any errors
}

// Check if the ESP32 is in service mode
fetch('/is_service_mode')
  .then(response => response.json())
  .then(data => {
    if (data.serviceMode) {
      document.getElementById('service-mode').style.display = 'block'; // Show buttons
    } else {
      document.getElementById('service-mode').style.display = 'none'; // Hide buttons
    }
  })
  .catch(error => console.error('Error:', error));