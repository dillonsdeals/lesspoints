// DOM Elements
const dealForm = document.getElementById('dealForm');
const cashPriceInput = document.getElementById('cashPrice');
const pointsUsedInput = document.getElementById('pointsUsed');
const taxesFeesInput = document.getElementById('taxesFees');
const analyzeBtn = document.getElementById('analyzeBtn');
const resultsContainer = document.getElementById('resultsContainer');
const results = document.getElementById('results');
const cppValue = document.getElementById('cppValue');
const verdict = document.getElementById('verdict');

// Error messages
const cashPriceError = document.getElementById('cashPriceError');
const pointsUsedError = document.getElementById('pointsUsedError');
const taxesFeesError = document.getElementById('taxesFeesError');

// Initialize the app
document.addEventListener('DOMContentLoaded', initApp);

function initApp() {
  // Set up event listeners
  dealForm.addEventListener('submit', handleFormSubmit);
  cashPriceInput.addEventListener('input', formatCurrencyInput);
  taxesFeesInput.addEventListener('input', formatCurrencyInput);
  
  // Add focus and blur event listeners for input styling
  const inputs = [cashPriceInput, pointsUsedInput, taxesFeesInput];
  inputs.forEach(input => {
    input.addEventListener('focus', handleInputFocus);
    input.addEventListener('blur', handleInputBlur);
  });
}

// Handle form submission
function handleFormSubmit(e) {
  e.preventDefault();
  
  // Reset error messages
  resetErrorMessages();
  
  // Validate inputs
  if (!validateInputs()) {
    return;
  }
  
  // Show loading state
  showLoading(true);
  
  // Simulate calculation delay (for UX purposes)
  setTimeout(() => {
    calculateAndDisplayResults();
    showLoading(false);
  }, 500);
}

// Validate all inputs
function validateInputs() {
  let isValid = true;
  
  // Validate cash price
  const cashPrice = parseCurrency(cashPriceInput.value);
  if (isNaN(cashPrice) || cashPrice <= 0) {
    cashPriceError.textContent = 'Please enter a valid cash price';
    isValid = false;
  }
  
  // Validate points
  const pointsUsed = parseInt(pointsUsedInput.value);
  if (isNaN(pointsUsed) || pointsUsed <= 0) {
    pointsUsedError.textContent = 'Please enter a valid number of points';
    isValid = false;
  }
  
  // Validate taxes and fees (can be 0 or positive)
  const taxesFees = parseCurrency(taxesFeesInput.value);
  if (isNaN(taxesFees) || taxesFees < 0) {
    taxesFeesError.textContent = 'Please enter a valid amount for taxes & fees';
    isValid = false;
  }
  
  return isValid;
}

// Reset all error messages
function resetErrorMessages() {
  cashPriceError.textContent = '';
  pointsUsedError.textContent = '';
  taxesFeesError.textContent = '';
}

// Show/hide loading state
function showLoading(isLoading) {
  if (isLoading) {
    analyzeBtn.classList.add('loading');
    analyzeBtn.disabled = true;
  } else {
    analyzeBtn.classList.remove('loading');
    analyzeBtn.disabled = false;
  }
}

// Calculate and display results
function calculateAndDisplayResults() {
  // Parse input values
  const cashPrice = parseCurrency(cashPriceInput.value);
  const pointsUsed = parseInt(pointsUsedInput.value);
  const taxesFees = parseCurrency(taxesFeesInput.value);
  
  // Calculate net cash value (what the points are saving you) and CPP
  const pointsValue = cashPrice - taxesFees;
  const cpp = pointsValue / pointsUsed;
  const cppCents = cpp * 100;
  
  // Display values
  cppValue.textContent = `${cppCents.toFixed(2)}¢`;
  
  // Set the rating verdict
  setVerdict(cpp);
  
  // Show results with animation
  showResults();
}

// Set the verdict text and style based on CPP value
function setVerdict(cpp) {
  let ratingText = '';
  let ratingClass = '';
  
  if (cpp < 0.01) {
    ratingText = 'Awful price, move on';
    ratingClass = 'verdict-poor';
  } else if (cpp >= 0.01 && cpp < 0.02) {
    ratingText = 'I\'d pass, not that great of a deal';
    ratingClass = 'verdict-average';
  } else if (cpp >= 0.02 && cpp < 0.03) {
    ratingText = 'This is a great deal, go for it!';
    ratingClass = 'verdict-good';
  } else if (cpp >= 0.03 && cpp < 0.05) {
    ratingText = 'This is an AMAZING deal, I\'d book it right now';
    ratingClass = 'verdict-great';
  } else if (cpp >= 0.05) {
    ratingText = 'AN ULTIMATE DEAL, BOOK THAT FLIGHT!!';
    ratingClass = 'verdict-ultimate';
  }
  
  // Remove all verdict classes
  verdict.className = 'verdict';
  
  // Add the appropriate verdict class
  verdict.classList.add(ratingClass);
  
  // Set the verdict text
  verdict.textContent = ratingText;
}

// Show results with animation
function showResults() {
  results.classList.remove('hidden');
  
  // Force a reflow before adding animation
  void results.offsetWidth;
  
  // Add animation class
  results.style.animation = 'scaleIn 0.3s forwards';
}

// Format currency input while typing
function formatCurrencyInput(e) {
  const input = e.target;
  let value = input.value;
  
  // Remove all non-numeric characters except decimal point
  value = value.replace(/[^\d.]/g, '');
  
  // Ensure only one decimal point
  const decimalPoints = (value.match(/\./g) || []).length;
  if (decimalPoints > 1) {
    const parts = value.split('.');
    value = parts[0] + '.' + parts.slice(1).join('');
  }
  
  // Format with 2 decimal places if there's a decimal point
  if (value.includes('.')) {
    const parts = value.split('.');
    // Limit to 2 decimal places
    if (parts[1].length > 2) {
      parts[1] = parts[1].substring(0, 2);
      value = parts[0] + '.' + parts[1];
    }
  }
  
  // Update input value
  input.value = value;
}

// Parse currency string to number
function parseCurrency(value) {
  if (!value) return 0;
  return parseFloat(value.replace(/[^\d.]/g, ''));
}

// Format number as currency string
function formatCurrency(value) {
  return '$' + value.toFixed(2);
}

// Handle input focus
function handleInputFocus(e) {
  e.target.parentElement.style.borderColor = 'var(--primary-color)';
}

// Handle input blur
function handleInputBlur(e) {
  e.target.parentElement.style.borderColor = '';
}
