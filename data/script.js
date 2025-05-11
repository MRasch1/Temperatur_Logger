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
  const now = new Date();

  // Format date as DD/MM/YYYY
  const day = String(now.getDate()).padStart(2, '0');
  const month = String(now.getMonth() + 1).padStart(2, '0'); // Month is 0-based
  const year = now.getFullYear();

  const formattedDate = `${day}/${month}/${year}`;
  const formattedTime = now.toLocaleTimeString(); // You can customize this too if needed

  const dateTimeLabel = `${formattedDate} ${formattedTime}`; // Combine date and time

  chartData.labels.push(dateTimeLabel); // Add formatted label
  chartData.datasets[0].data.push(temperature); // Add temperature data

  // Limit to 30 data points
  if (chartData.labels.length > 30) {
    chartData.labels.shift();
    chartData.datasets[0].data.shift();
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